/**@mainpage Simulation of neural architectures.
This code is based on ..
@version 2.0
@author whoever
@date whenever
*/



#ifndef __NO_MPI

#include "mpi.h"

#endif

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <cstring>
#include <string>

#include "utils.h"
#include "Network.h"
#include "Protocol.h"

#include "randgen.hpp"
#include "r250.hpp"

R250 *r250;

using namespace std;

extern int flagverbose; //extern significa que la variable se declaro en otro archivo.

int NumberOfTrials;
int NumberOfTraces;
long fixedSeed;

bool flagSaveDetailedSpikes;
bool flagSaveTraces;
bool flagFixedSeed;


void parseArguments(int argc, char **argv);

int main(int argc, char **argv) {

	// Default values for flags and options.
	flagSaveDetailedSpikes = true;
	flagSaveTraces = false;
	flagFixedSeed = false;
	NumberOfTrials = 1;
	NumberOfTraces = 0;
	fixedSeed = time(0);

	double dt = 5e-2;
	char delay_in_dt = 5;
	int Trial = 0;
	double Time = 0;
	float nextTimeEvent;
#ifndef __NO_MPI
	int rank, size;
	unsigned long int seeds[MAX_NODES];
	unsigned long int localseed;
#endif

	flagverbose = 0;

	cout << fixed;
	cout << setprecision(6);

	parseArguments(argc, argv);


#ifndef __NO_MPI
	MPI::Init(argc,argv);
	rank = MPI::COMM_WORLD.Get_rank();
	size = MPI::COMM_WORLD.Get_size();

	if (rank==0){
		r250 = new R250(fixedSeed);
		std::cout << "Rank 0 informing: R250 initialized with seed " << fixedSeed << endl;
		for (int i=0;i<size;i++){
			seeds[i]=r250->rani();
		}
	}

	MPI::COMM_WORLD.Scatter(seeds,1,MPI_UNSIGNED_LONG,&localseed,1,MPI_UNSIGNED_LONG,0);
	if (rank!=0) {
		r250 = new R250(localseed);
		std::cout << "Node " << rank << ": R250 initialized with seed " << localseed << endl;
	}

	//file names
	string buffer;
	ifstream file;
	int line;
	
	file.clear();
	file.open("rank_mapping.txt");
	if(!file.is_open()) { report("ERROR:  Unable to read extension file\n");}

	line = -1;
	while(!file.eof() && line<rank) {
		getline(file,buffer);
		line++;
	}
		
	stringstream networkconfFileName;
	networkconfFileName<<buffer<<"network.conf";

	stringstream networkproFileName;
	networkproFileName<<buffer<<"network.pro";

	//fin file names
#else
	stringstream networkconfFileName;
	networkconfFileName<<"network.conf";

	stringstream networkproFileName;
	networkproFileName<<"network.pro";

	r250 = new R250(fixedSeed);
	std::cout << "R250 initialized with seed " << fixedSeed << endl;
#endif
	
	//ariel
	for (Trial=0;Trial<NumberOfTrials;Trial+=1) {

		Network net(networkconfFileName.str(),dt);
		net.connectPopulations(delay_in_dt);

		//debug
		ofstream debugout;
		debugout.open ("debug.dat");
		//int j;
		//for (j=0;j<net.Pop[0].Cell[0].Naxonals;j++){
			//debugout << net.Pop[0].Cell[0].Axonals[j]->EfficacyMod << endl;
            //debugout << net.Pop[0].Cell[0].Axonals[2000]->getLSsumand() << endl;
		//}
		//debugout.close();
		//fin debug
		

		Protocol protocol(networkproFileName.str());
		protocol.parseProtocol(net);
		protocol.parseProtocol2("events.diff",net);
		protocol.sortEvents();
		
		stringstream RatesFileName;
		stringstream detailedSpikesFile;
		stringstream tracesV;
#ifndef __NO_MPI
		// Configure FREQS file for saving results.
		RatesFileName << buffer << "popfreqs" << Trial << "_" << rank << ".dat";
		net.openSpikesFreqFile(RatesFileName.str());

		// Configure detailed spikes file.
		if (flagSaveDetailedSpikes) {
			detailedSpikesFile << "trial" << Trial << "_rank" << rank << "_spikes" << ".dat";
			net.openDetailedSpikesFile(detailedSpikesFile.str());
		}

		// Configure file for saving traces of variables.
		if (flagSaveTraces) {
			tracesV << "trial" << Trial << "_rank" << rank << "_traces_V" << ".dat";
			net.openTracesVFile(tracesV.str(), NumberOfTraces);
		}
#else
		RatesFileName << "popfreqs" << Trial << ".dat";
		net.openSpikesFreqFile(RatesFileName.str());

		if (flagSaveDetailedSpikes) {
			detailedSpikesFile << "trial" << Trial << "_spikes" << ".dat";
			net.openDetailedSpikesFile(detailedSpikesFile.str());
		}

		if (flagSaveTraces) {
			tracesV << "trial" << Trial << "_traces_V" << ".dat";
			net.openTracesVFile(tracesV.str(),NumberOfTraces);
		}
#endif

		protocol.resetEventIndex();
		nextTimeEvent = protocol.getNextTimeEvent();

		long stepCounter = 0;

		for (Time=0;Time<protocol.trialDuration;Time+=dt) {
			net.SimulateOneTimeStep(float(Time));
            
            		debugout << net.Pop[0].Cell[0].Axonals[0]->getLSsumand() << endl;
            
			// Handle all the events
			while(Time >= nextTimeEvent) {
				protocol.getCurrentEvent().handleEvent(net);
				if (protocol.advanceEventIndex())
					nextTimeEvent = protocol.getNextTimeEvent();
					//SaveSpikes(1,dt,Time,Pop,Npop);
			} 

			if((stepCounter % STEPS_FOR_SAVING_FREQS)==0) net.saveSpikesFreq(float(Time));
			if((stepCounter % STEPS_FOR_PRINTING_FREQS)==0) net.printSpikesFreq(float(Time), cout);
			if (flagSaveDetailedSpikes) net.saveDetailedSpikes(float(Time));
			if (flagSaveTraces) net.saveTracesV(float(Time),NumberOfTraces);
	
			stepCounter++;
		}

		report("\rEnd of the trial\n");

		cout << "End of Trial " << Trial+1 << " of " << NumberOfTrials <<"\n";

		net.closeSpikesFreqFile();
		if (flagSaveDetailedSpikes) net.closeDetailedSpikesFile();
		if (flagSaveTraces) net.closeTracesVFile();
		
		//debug
        debugout.close();

	}
    
    
    
	delete r250;

#ifndef __NO_MPI
	MPI::Finalize();
#endif
	closeReport();
	return EXIT_SUCCESS;
}


void parseArguments(int argc, char **argv){
	if(argc>1) {

		do {
			if(strncmp(argv[argc-1],"-v",2)==0) { flagverbose=1; argc--; continue; }

			if(strncmp(argv[argc-1],"-h",2)==0) {
				cout << "realsimu - Ver. 0.8\n Usage:\n-h  : this help\n-v  : verbose mode\n-t# : number of saved traces per population" << endl;
				cout << "-T# : number of trials (the network is the same for each trial, the realization of the ext noise changes)" << endl;
				cout << "-ns : spikes and traces are not saved. Only the mean frequencies are saved for each trial" << endl;
				exit(EXIT_SUCCESS);
			}

			if(strncmp(argv[argc-1],"-t",2)==0) { 
				NumberOfTraces=atoi(&argv[argc-1][2]);
				if (NumberOfTraces>0) flagSaveTraces=true;
				cout << "Number of saved traces: " << NumberOfTraces << endl;
				argc--; continue;
			}

			if(strncmp(argv[argc-1],"-s",2)==0) {
				flagFixedSeed = true;
				fixedSeed = atoi(&argv[argc-1][2]);
				cout << "Seed for random generator: " << fixedSeed << endl;
				argc--; continue;
			}

			if(strncmp(argv[argc-1],"-ns",3)==0) {
				flagSaveDetailedSpikes=false;
				cout << "Spikes are not saved" << endl;
				argc--; continue;
			}

			if(strncmp(argv[argc-1],"-T",2)==0) {
				NumberOfTrials=atoi(&argv[argc-1][2]);
				cout << "Number of trials: " << NumberOfTrials << endl;
				argc--; continue;
			}
			cout << "ERROR: unrecognized option: " << argv[argc] << endl;
			argc--;
		} while(argc>1);
	}
}


/*int SaveSpikes(int eventflag, const float dt, const float Time, const Population* Pop, const int Npop)
{
	static int InitFlag=1;
	static FILE *devspikes[MAXP],*devfreqs;
	static FILE *extFrequency; //AZ
	static float meanfreqs[MAXP];
	static float timelastevent;
	static float meanfreqsbetweenevents[MAXP];
	static int counter;
	static int lasttrial=0;
	int i,p,TempPointer;
	char TempName[100];

	// initialize if it is the first call
	if(InitFlag || (lasttrial!=CurrentTrial)) {

		// if there is a new trial, close all the files of the previous trial
		if(lasttrial!=CurrentTrial) {
			if (diffFile>0)  
			{
				fclose(extFrequency);
			}

			if(FlagSaveAllSpikes)
			{
				for(p=0;p<Npop;p++)
				{
					fclose(devspikes[p]);	
				}
			}
			lasttrial=CurrentTrial;
			// and then open th new ones
		}

		// open all files
		printf("\n Time (ms) ");

		//AZ
		if (diffFile>0)
		{
			sprintf(TempName,"extFreq%d.dat",CurrentTrial);
			extFrequency=fopen(TempName,"w");
		}
		//

		for(p=0;p<Npop;p++)
		{
			sprintf(TempName,"pop%d_%d.dat",p+1,CurrentTrial);
			if(FlagSaveAllSpikes)
			{
				devspikes[p]=fopen(TempName,"w");
				if(devspikes[p]==NULL) return 0;
			}
			meanfreqs[p]=0.;
			meanfreqsbetweenevents[p]=0.;

			printf("%12s",Pop[p].Label);
		}
		timelastevent=0.;

		sprintf(TempName,"popfreqs%d.dat",CurrentTrial);
		devfreqs=fopen(TempName,"w");
		if(devfreqs==NULL) return 0;
		InitFlag=0;    
		counter=0;
		printf("\n---------------------------------------------------------------\n");
	}//cierra la inicializacion

	if((counter % STEPS_FOR_SAVING_FREQS)==0) fprintf(devfreqs,"%f\t",Time);

	if((counter % STEPS_FOR_PRINTIG_FREQS)==0) printf("%7.1f ms",Time);

	if (diffFile>0)
	{
		if((counter % STEPS_FOR_SAVING_EXT)==0) fprintf(extFrequency,"%f\t",Time); //AZ
	}

	for(p=0;p<Npop;p++)
	{
		TempPointer=Pop[p].CTableofSpikes-1;
		if(TempPointer<0) TempPointer=MAXDELAYINDT-1;

		meanfreqsbetweenevents[p]+=(float)Pop[p].NTableofSpikes[TempPointer]/(float)Pop[p].Ncells/dt*1000.;

		meanfreqs[p]+=dt/TIMEWINDOWFORFREQ*(-meanfreqs[p]+(float)Pop[p].NTableofSpikes[TempPointer]/(float)Pop[p].Ncells/dt*1000.); // compute mean freq in Hz on a time window of 10 ms

		if((counter % STEPSFORPRINTIGFREQS)==0) printf("%12.1f",meanfreqs[p]);
		if((counter % STEPSFORSAVINGFREQS)==0) fprintf(devfreqs,"%f\t",meanfreqs[p]); // mean frequency in Hz per neuron

		if (diffFile>0)
		{
			if((counter % STEPSFORSAVINGEXT)==0) fprintf(extFrequency,"%f\t",PopD[p].MeanExtMuS[0]/(.001*PopD[p].MeanExtEff[0]*PopD[p].MeanExtCon[0]*PopD[p].Tau[0])); //AZ External input, 
		}
		// calculated from the mean external conductance to take into account the deltas included in the diff file. External "spiking" inputs are 
		// NOT included. 


		if(FlagSaveAllSpikes)
		{
			for(i=0;i<Pop[p].NTableofSpikes[TempPointer];i++) {
				fprintf(devspikes[p],"%d\t%f\n",Pop[p].TableofSpikes[TempPointer][i],Time);
			}
		}
	}

	if((counter % STEPSFORSAVINGFREQS)==0) fprintf(devfreqs,"\n");
	if((counter % STEPSFORPRINTIGFREQS)==0) printf("\n");

	if (diffFile>0)
	{
		if((counter % STEPSFORSAVINGEXT)==0) fprintf(extFrequency,"\n"); //AZ
	}

	if((counter % STEPSFORFLUSHING)==0) {
		if(FlagSaveAllSpikes)
		{
			for(p=0;p<Npop;p++)
			{
				fflush(devspikes[p]);
			}
		}
		fflush(devfreqs);
	}

	if(eventflag) {

		printf("Average:  ");
		for(p=0;p<Npop;p++)
		{
			printf("%12.1f",meanfreqsbetweenevents[p]/(Time-timelastevent)*dt);
			meanfreqsbetweenevents[p]=0.;
		}
		printf("\n");
		fflush(stdout);
		timelastevent=Time;
	}
	counter++;
	return 1;
}
*/
