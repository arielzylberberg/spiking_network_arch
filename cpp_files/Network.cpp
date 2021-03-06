#include "Network.h"

//#include "erf.cpp"



Network::Network(const string& ncfn, const double deltat) {
	// Default value for saveAllSpikes. We don't want to much files "just-in-case".
	//saveAllSpikes = false;

	// Load network descripton and reserve memory.
	network_conf_fileName = ncfn;
	dt = deltat;

	// Load description and reserve memory.
	assert(loadNetworkDescription()==LOAD_NETWORK_OK);
}

Network::~Network(){
	delete[] Pop;
}

int Network::loadNetworkDescription() {
	ifstream net_conf;
	string buffer, sub_buffer;
//	string receptorLabel[MAX_POPULATIONS][MAX_RECEPTORS];
//	int Nreceptors[MAX_POPULATIONS];
	
	int currentpopflag=0;
	int line;
	int currentpop=-1;

	// Population pointer for new population creation.
	vector<PopulationDescr*> PopD;
	PopulationDescr *popDescr;

	// FIRST PASSAGE
	// -------------------------------------------------------------------
	// the parser has to go over the file twice: the first time reads all
	// the labels and initializes the number of populations and the number
	// of receptors
	report("Parsing network configuration... first passage\n");
	
	net_conf.open(network_conf_fileName.c_str());
	if (!net_conf.is_open()){
		report("ERROR:  Unable to read configuration file\n");
		return LOAD_NETWORK_ERROR;
	}

	Npop=0;
	line=-1;

	while (!net_conf.eof()){ 
		getline(net_conf,buffer);
		removeComments(trim(buffer));

		line++;
		// commands for defining a new population
		if (buffer.compare(0,17,"NeuralPopulation:")==0) {
			currentpopflag=1;
			buffer = buffer.substr(17,string::npos);
			popDescr = new PopulationDescr(trim(buffer));
			PopD.push_back(popDescr);
			//report("Population: %s\n", PopD[Npop].Label.c_str());
			continue;
		}

		if (buffer.compare(0,19,"EndNeuralPopulation")==0) {
			Npop++;
			continue;
		}

		// command for defining a receptor
		if (buffer.compare(0,9,"Receptor:")==0) {
			PopD[Npop]->ReceptorLabel[PopD[Npop]->Nreceptors] = buffer.substr(9,string::npos);
			//report("Receptor %d: %s\n", PopD[Npop].Nreceptors,
			//	PopD[Npop].ReceptorLabel[PopD[Npop].Nreceptors].c_str());
		}

		if (buffer.compare(0,11,"EndReceptor")==0) {
			PopD[Npop]->Nreceptors++;
			assert(PopD[Npop]->Nreceptors<=MAX_RECEPTORS);
		}
	}

	// fclose(devconf);
	net_conf.close();

	// Now that we have the number of populations, each label and receptors, we create the population objects.
	Pop = new Population[Npop];
	for (int i = 0;i<Npop;i++){
		Pop[i].Label.assign(PopD[i]->Label);
		Pop[i].Nreceptors = PopD[i]->Nreceptors;
		for (int j=0;j<PopD[i]->Nreceptors;j++){
			Pop[i].receptor[j].receptorLabel = PopD[i]->ReceptorLabel[j];
		}
	}

	// Second passage: now all the parameters and the target populations are parsed
	// ----------------------------------------------------------------------------

	report("Parsing network configuration... second passage\n");

	net_conf.clear();
	net_conf.open(network_conf_fileName.c_str());
	if (!net_conf.is_open()){
		report("ERROR:  Unable to read configuration file\n");
		return LOAD_NETWORK_ERROR;
	}

	line= -1;
	while (!net_conf.eof()){ 
		getline(net_conf,buffer);
		removeComments(trim(buffer));

		line++;
		if (buffer.compare(0,17, "NeuralPopulation:")==0) {
			sub_buffer.clear();
			sub_buffer = buffer.substr(17,string::npos);
			trim(sub_buffer);
			currentpop=PopulationCode(sub_buffer);
			if (currentpop==INVALID_POPULATION_CODE) {
				printf("Unknown population [%s]: line %d\n", sub_buffer.c_str(), line);
				return LOAD_NETWORK_ERROR;
			}
			Pop[currentpop].loadPopulation(*this,net_conf);
			continue;
		}


	}

	net_conf.close();

	// Delete temporary populations description
	for (unsigned int i=0; i < PopD.size();i++){
		delete PopD[i];
	}

	return LOAD_NETWORK_OK;
}


int Network::connectPopulations(const char delay_in_dt) {
	int targetPop;
	unsigned short connId;
	int targetNeuron;
	int i, k, na, p,units_distance;

	float sourcePos, targetPos,normDist,tmp1,sigma,jplus;
	float tEfficacyModAux;
	float targetPosX,targetPosY,nRowsTarget,nRowsSource,sourcePosX,sourcePosY,normDistX,normDistY;
	float connProb;

	vector<short> tNeuron;
	vector<short> tConnId;
	vector<float> initLastCond;
	vector<float> tEfficacyMod;
	vector<char> tSTPcode;
	vector<char> tSTDcode;
	vector<char> tSTFcode;

	

	report("\nGenerating network...\n");

	// loop over all the populations
	for (p=0; p<Npop; p++) {

		report("  - Number of cells: %d\n", Pop[p].Ncells);

		// init auxiliary variables like the tables of spikes
		if (delay_in_dt>MAXDELAYINDT) {
			printf("ERROR: delay too long. Increase MAXDELAYINDT and recompile\n");
			return LOAD_NETWORK_ERROR;
		}
		Pop[p].CTableofSpikes=delay_in_dt;
		Pop[p].DTableofSpikes=0;
		for (k=0; k<MAXDELAYINDT; k++) {
			Pop[p].NTableofSpikes[k]=0;
		}
		
		// generate neurons and their axonal trees
		for (i=0; i<Pop[p].Ncells; i++) {
			tConnId.clear(); 
			tNeuron.clear();
			tEfficacyMod.clear();
			tSTPcode.clear();
			tSTFcode.clear();
			tSTDcode.clear();
			initLastCond.clear();

			// Axonal tree -------------------------------
			// loop on target populations

			for (connId=0; connId<Pop[p].conn.size(); connId++) {

				switch(Pop[p].conn[connId].connectivityCode)
				{
					case (1): //random connection with probability "Connectivity"

						targetPop = Pop[p].conn[connId].targetPopulation;
						na=Pop[p].Cell[i].Naxonals; // auxiliary variable to speed up

						for (targetNeuron = 0; targetNeuron < Pop[targetPop].Ncells; targetNeuron++) {
							if (targetPop==p) {
								if (i==targetNeuron)
									continue; // avoid self connections
							}

							if (r250->ranf() < Pop[p].conn[connId].Connectivity) { // the connection exists
								tConnId.push_back(connId); 
								tNeuron.push_back(targetNeuron); 
								tSTPcode.push_back(Pop[p].conn[connId].stp_code);
								tSTFcode.push_back(Pop[p].conn[connId].stfcode);
								tSTDcode.push_back(Pop[p].conn[connId].stdcode);
								tEfficacyMod.push_back(1.0);

								if (Pop[targetPop].receptor[Pop[p].conn[connId].targetReceptor].receptorLabel.compare(0, 4, "NMDA")==0) {
									initLastCond.push_back(0.); // initial LastConductance (for NMDA saturation)
								} else {
									initLastCond.push_back(-1.); // not an NMDA type (so, no saturation)
								}
								na++;
								Pop[p].Cell[i].Naxonals++;
							}
						} // end for tn
					break;

					case (2): //compte-structured

						targetPop = Pop[p].conn[connId].targetPopulation;
						na=Pop[p].Cell[i].Naxonals; // auxiliary variable to speed up

						for (targetNeuron = 0; targetNeuron < Pop[targetPop].Ncells; targetNeuron++) {
							if (targetPop==p) {
								if (i==targetNeuron)
									continue; // avoid self connections
							}
							
							targetPos = (float) targetNeuron/Pop[targetPop].Ncells;
							sourcePos = (float) i/Pop[p].Ncells;

							normDist = min((float)abs(targetPos-sourcePos),(float)1.0-abs(targetPos-sourcePos));

							if (r250->ranf() < Pop[p].conn[connId].Connectivity) { // the connection exists
								tConnId.push_back(connId); 
								tNeuron.push_back(targetNeuron); 
								tSTPcode.push_back(Pop[p].conn[connId].stp_code);
								tSTFcode.push_back(Pop[p].conn[connId].stfcode);
								tSTDcode.push_back(Pop[p].conn[connId].stdcode);
								
								jplus=Pop[p].conn[connId].jplus;
								tmp1=Pop[p].conn[connId].jmin;
								sigma=Pop[p].conn[connId].sigma;

								tEfficacyModAux=(float) tmp1+(jplus-tmp1)*exp((float)-0.5*normDist*normDist/(sigma*sigma));

								tEfficacyMod.push_back(tEfficacyModAux);

								if (Pop[targetPop].receptor[Pop[p].conn[connId].targetReceptor].receptorLabel.compare(0, 4, "NMDA")==0) {
									initLastCond.push_back(0.); // initial LastConductance (for NMDA saturation)
								} else {
									initLastCond.push_back(-1.); // not an NMDA type (so, no saturation)
								}
								na++;
								Pop[p].Cell[i].Naxonals++;
							}
							
						} // end for tn

					break;

					case (3): //probabilistic-structured

						targetPop = Pop[p].conn[connId].targetPopulation;
						na=Pop[p].Cell[i].Naxonals; // auxiliary variable to speed up

						for (targetNeuron = 0; targetNeuron < Pop[targetPop].Ncells; targetNeuron++) {
							if (targetPop==p) {
								if (i==targetNeuron)
									continue; // avoid self connections
							}
							
							targetPos = (float) targetNeuron/Pop[targetPop].Ncells;
							sourcePos = (float) i/Pop[p].Ncells;

							normDist = min((float)abs(targetPos-sourcePos),(float)1.0-abs(targetPos-sourcePos));

							jplus=Pop[p].conn[connId].jplus;
							tmp1=Pop[p].conn[connId].jmin;
							sigma=Pop[p].conn[connId].sigma;

							connProb=(float) tmp1+(jplus-tmp1)*exp((float)-0.5*normDist*normDist/(sigma*sigma));

							if (r250->ranf() < connProb) { // the connection exists
								tConnId.push_back(connId); 
								tNeuron.push_back(targetNeuron); 
								tSTPcode.push_back(Pop[p].conn[connId].stp_code);
								tSTFcode.push_back(Pop[p].conn[connId].stfcode);
								tSTDcode.push_back(Pop[p].conn[connId].stdcode);
								
								tEfficacyMod.push_back(1.0);

								if (Pop[targetPop].receptor[Pop[p].conn[connId].targetReceptor].receptorLabel.compare(0, 4, "NMDA")==0) {
									initLastCond.push_back(0.); // initial LastConductance (for NMDA saturation)
								} else {
									initLastCond.push_back(-1.); // not an NMDA type (so, no saturation)
								}
								na++;
								Pop[p].Cell[i].Naxonals++;
							}
							
						} // end for tn

					break;


					case (4): //two-dimensional recurrent compte-structured

						targetPop = Pop[p].conn[connId].targetPopulation;
						na=Pop[p].Cell[i].Naxonals; // auxiliary variable to speed up

						for (targetNeuron = 0; targetNeuron < Pop[targetPop].Ncells; targetNeuron++) {
							if (targetPop==p) {
								if (i==targetNeuron)
									continue; // avoid self connections
							}
							
							nRowsTarget = floor(sqrt((float) Pop[targetPop].Ncells) + 0.5); //number of rows in 2D structure
							targetPosX = (float) fmod((float) targetNeuron,(float) nRowsTarget)/nRowsTarget;
							targetPosY = (float) fabs((float) targetNeuron/(float) nRowsTarget)/nRowsTarget;
							
							nRowsSource = floor(sqrt((float) Pop[p].Ncells) + 0.5);
							sourcePosX = (float) fmod((float) i,(float) nRowsSource)/nRowsSource;
							sourcePosY = (float) fabs((float) i/(float) nRowsSource)/nRowsSource;

							normDistX = min((float)abs(targetPosX-sourcePosX),(float)1.0-abs(targetPosX-sourcePosX));
							normDistY = min((float)abs(targetPosY-sourcePosY),(float)1.0-abs(targetPosY-sourcePosY));
							
							normDist = sqrt((float) normDistX*normDistX + normDistY*normDistY);

							if (r250->ranf() < Pop[p].conn[connId].Connectivity) { // the connection exists
								tConnId.push_back(connId); 
								tNeuron.push_back(targetNeuron); 
								tSTPcode.push_back(Pop[p].conn[connId].stp_code);
								tSTFcode.push_back(Pop[p].conn[connId].stfcode);
								tSTDcode.push_back(Pop[p].conn[connId].stdcode);
								
								jplus=Pop[p].conn[connId].jplus;
								tmp1=Pop[p].conn[connId].jmin;
								sigma=Pop[p].conn[connId].sigma;

								tEfficacyModAux=(float) tmp1+(jplus-tmp1)*exp((float)-0.5*normDist*normDist/(sigma*sigma));

								tEfficacyMod.push_back(tEfficacyModAux);

								if (Pop[targetPop].receptor[Pop[p].conn[connId].targetReceptor].receptorLabel.compare(0, 4, "NMDA")==0) {
									initLastCond.push_back(0.); // initial LastConductance (for NMDA saturation)
								} else {
									initLastCond.push_back(-1.); // not an NMDA type (so, no saturation)
								}
								na++;
								Pop[p].Cell[i].Naxonals++;
							}
							
						} // end for tn

					break;


					default:
						cout << "The specified connection code does not exists" << endl; 
				} //switch

			} // for connId
			//

			// After loading axonals from PopD, send them to Pop. 
			if (tConnId.size()>0)
				Pop[p].Cell[i].loadAxonals(&tConnId[0], &tNeuron[0], &tEfficacyMod[0] ,&(tSTPcode[0]) ,&(tSTFcode[0]), &(tSTDcode[0]), &initLastCond[0]);

		} // end for i

	} // end for p
	

	report("Network generated\n");

	return GENERATE_NETWORK_OK;
}

// returns the code of the population with name s (-1 in case of error)
int Network::PopulationCode(const string& s) const{
	int p;

	for(p=0;p<Npop;p++)	{
		if (Pop[p].Label.compare(s)==0) {
			return p;
		}
	}
	return INVALID_POPULATION_CODE;
}

void Network::closeSpikesFreqFile(){
	freqsFile.close();
}

bool Network::openSpikesFreqFile(const string& freqsFileName){
	bool result = true;

	freqsFile.clear();
	freqsFile.open(freqsFileName.c_str());
	freqsFile << setprecision(6);
	freqsFile << fixed;
	result = result && (freqsFile.good() && freqsFile.is_open());

	return result;
}

void Network::saveSpikesFreq(const float Time){
	int i;

	freqsFile << Time << "\t";

	// save mean frequency in Hz per neuron
		for (i=0;i<Npop;i++){
			freqsFile << Pop[i].meanFreqs << "\t";
		}

	freqsFile << endl;
}

void Network::printSpikesFreq(const float Time, ostream& out){
	int i;

	out << Time << "\t";

	// save mean frequency in Hz per neuron
	for (i=0;i<Npop;i++){
		out << Pop[i].meanFreqs << "\t";
	}
	
	out << endl;
}

int Network::SimulateOneTimeStep(const float Time)
{
	short i;
	int p,j,r,sourceneuron;
	int tn,tp,tr, cId;
	float s,auxDummy;
	float Vaux; // auxiliary V: during the emission of the spike V is set artificially to 0. This is bad for the reversal potential

	// DEBUG
	//float value=0;
	// END DEBUG

	// Compute the decay of the total conductances and add external input
	// ------------------------------------------------------------------

	for(p=0;p<Npop;p++)
	{
		for(i=0;i<Pop[p].Ncells;i++)
		{
			Neuron* myCell = &(Pop[p].Cell[i]);
			for(r=0;r<Pop[p].Nreceptors;r++)
			{
				s=myCell->ExtSigmaS[r];
				if(s!=0.) // to optimize
				{
					myCell->ExtS[r]+=float(dt)*Pop[p].receptor[r].OneOverTau*(-myCell->ExtS[r] + myCell->ExtMuS[r])
						+s*Pop[p].receptor[r].SqrtConstantTau*r250->gaussian();

					//Aclaracion:  Pop[p].Cell[i].SqrtConstantTau[r]=sqrt(dt*2./Pop[p].Cell[i].Tau[r]);
					//Aclaracion:  Pop[p].Cell[i].OneOverTau[r]=1/Pop[p].Cell[i].Tau[r];
				}
				else {
					myCell->ExtS[r]+=float(dt)*Pop[p].receptor[r].OneOverTau*(-myCell->ExtS[r]+myCell->ExtMuS[r]);
				}

				myCell->LS[r] *= Pop[p].receptor[r].ExpConstantTau; // decay
				//Aclaracion:  Pop[p].Cell[i].ExpConstantTau[r]=exp(-dt/Pop[p].Cell[i].Tau[r]);

			}
			//Calcium coentration decay, for spike frequency adaptation
			if (Pop[p].SFACode==1)
				myCell->CA*=Pop[p].ExpConstantTca;
				//Aclaracion:  Pop[p].Cell[i].ExpConstantTca=exp(-dt/Pop[p].Cell[i].tca);

		}
	}

	// Update the total conductances (changes provoked by the spikes)
	// --------------------------------------------------------------
	for(p=0;p<Npop;p++)
	{
		// loop over all the spikes emitted at time t-delay (they are received now)
		for(i=0;i<Pop[p].NTableofSpikes[Pop[p].DTableofSpikes];i++)
		{
			sourceneuron=Pop[p].TableofSpikes[Pop[p].DTableofSpikes][i];

			// for each spike, loop over the target conductances
			for(j=0;j<Pop[p].Cell[sourceneuron].Naxonals;j++)
			{
				cId = Pop[p].Cell[sourceneuron].Axonals[j]->connectionId;
				tn=Pop[p].Cell[sourceneuron].Axonals[j]->TargetNeuron;
				tp=Pop[p].conn[cId].targetPopulation;
				tr=Pop[p].conn[cId].targetReceptor;

				Pop[p].Cell[sourceneuron].Axonals[j]->decayPlasticity(Time-Pop[p].Cell[sourceneuron].PTimeLastSpike, Pop[p].conn[cId]);

				Pop[p].Cell[sourceneuron].Axonals[j]->decayConductance(Time-Pop[p].Cell[sourceneuron].PTimeLastSpike, Pop[tp].receptor[tr]);

				Pop[tp].Cell[tn].LS[tr] += (Pop[p].conn[cId].Efficacy * Pop[p].Cell[sourceneuron].Axonals[j]->EfficacyMod * Pop[p].Cell[sourceneuron].Axonals[j]->getLSsumand());

				Pop[p].Cell[sourceneuron].Axonals[j]->updateConductance();

				Pop[p].Cell[sourceneuron].Axonals[j]->updatePlasticity(Pop[p].conn[cId]);

			} 
		}
	}

	// Update the neuronal variables
	// -----------------------------

	for(p=0;p<Npop;p++)
	{
		Pop[p].NTableofSpikes[Pop[p].CTableofSpikes]=0; // reset the number of emitted spikes for pop p
		for(i=0;i<Pop[p].Ncells;i++)
		{
			Neuron* myCell = &(Pop[p].Cell[i]);
			if (Pop[p].Dummy!=1)
			{

				if(myCell->V > Pop[p].Threshold) 
				{
					myCell->V = Pop[p].ResetPot; // special state after emission
					myCell->RefrState--;
					continue;
				}

				// refractory period
				if(myCell->RefrState) {
					myCell->RefrState--;
					continue;
				}

				// decay
				myCell->V += -float(dt) * Pop[p].OneOverTaum * (myCell->V - Pop[p].RestPot);
				//DEFINIR myCell->OneOverTaum=1/myCell->Taum;

				// for SFA
				if (Pop[p].SFACode==1)
					myCell->V += -float(dt) * Pop[p].gAHP * float(0.001) * Pop[p].OneOverC * myCell->CA * (myCell->V - Pop[p].Vk);
				//DEFINIR myCell->OneOverC=1/myCell->C;

				Vaux=myCell->V;
				if ( Vaux > Pop[p].Threshold)  Vaux = Pop[p].Threshold;

				// now add the synaptic currents (the factor 1/1000 is needed to convert nS/nF to 1/ms)
				for(r=0;r<Pop[p].Nreceptors;r++)
				{
					if(Pop[p].receptor[r].MgFlag) { // magnesium block
						myCell->V += float(dt) * (Pop[p].receptor[r].RevPots-Vaux)* float(.001)*(myCell->LS[r]+myCell->ExtS[r])
							*Pop[p].OneOverC/((float)1.+exp(-(float)0.062*Vaux)/(float)3.57);

					} else {
						myCell->V+=float(dt)*(Pop[p].receptor[r].RevPots-Vaux)*float(.001)*(myCell->LS[r]+myCell->ExtS[r])
							*Pop[p].OneOverC;


					}
				}

			} else { //dummy population

				//impose V > threeshold if there is a spike
				auxDummy = exp(-float(dt) * float(.001) * Pop[p].DummyOutputRate);
				if (drand49()>auxDummy)
					myCell->V = Pop[p].Threshold+1; 
				else
					myCell->V = Pop[p].Threshold-1; 

			}  

			// spike condition
			if(myCell->V > Pop[p].Threshold) {

				// a spike is emitted
				Pop[p].TableofSpikes[Pop[p].CTableofSpikes][Pop[p].NTableofSpikes[Pop[p].CTableofSpikes]]=i;
				if(Pop[p].NTableofSpikes[Pop[p].CTableofSpikes]<MAXSPIKESINDT-1) Pop[p].NTableofSpikes[Pop[p].CTableofSpikes]++;
				else cout << "\nERROR: too many spikes in a dt (change MAXSPIKESINDT and recompile)\n";

				myCell->resetAfterSpike(Time,Pop[p].ResetPot,int(Pop[p].RefPeriod/dt));

				//update calcium concentration for SFA
				if (Pop[p].SFACode==1)
					myCell->CA+=Pop[p].dCA;

			}
		}
	}

	// Update the pointers of the table of spikes, and calculate mean frequencies.
	// ---------------------------------------------------------------

	for(p=0;p<Npop;p++)
	{
		Pop[p].CTableofSpikes++; if(Pop[p].CTableofSpikes>MAXDELAYINDT-1) Pop[p].CTableofSpikes=0;
		Pop[p].DTableofSpikes++; if(Pop[p].DTableofSpikes>MAXDELAYINDT-1) Pop[p].DTableofSpikes=0;

		Pop[p].updateFreqs(float(dt));
	}

	return 0;
}

long Network::calculateMemory(){
	long res = 0;

	res += sizeof(Network);
	res += network_conf_fileName.capacity() + network_pro_fileName.capacity();

	for (int i = 0 ; i<Npop;i++)
		res += Pop[i].calculateMemory();

	return res;
}

bool Network::openDetailedSpikesFile(const string& spikesFileName){
	bool result;
	spikesFile.open(spikesFileName.c_str());
	result = spikesFile.is_open() && spikesFile.good();
	if (result) 
		spikesFile << "Time \tPop \tNeuron" << endl;
	return result;
}

void Network::closeDetailedSpikesFile(){
	spikesFile.close();
}


void Network::saveDetailedSpikes(const float Time){
	for (int i=0;i<Npop;i++){
		Pop[i].saveAllSpikes(spikesFile, i, Time);
	}
}

bool Network::openTracesVFile(const string& fileName, const int numberOfTraces){
	bool result;
	tracesVFile.open(fileName.c_str());
	result = tracesVFile.is_open() && tracesVFile.good();
	if (result) {
		tracesVFile << "Time";	
		for (int i = 0; i < this->Npop;i++){
			for (int j = 0; j < numberOfTraces;j++){
				tracesVFile << "\tP"<<i<<"N"<<j;
			}
		}	
		tracesVFile << endl;
	}
	return result;
}
void Network::closeTracesVFile(){
	tracesVFile.close();
}
void Network::saveTracesV(const float Time, const int numberOfTraces){
	tracesVFile << Time;
	for (int i = 0; i < this->Npop;i++){
		Pop[i].saveTracesV(tracesVFile,numberOfTraces);
	}	
	tracesVFile << endl;
}
