#ifndef POPULATION_H_
#define POPULATION_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <vector>

#include "constants.h"
#include "utils.h"
#include "Neuron.h"
#include "Connection.h"
#include "Receptor.h"

#pragma pack(push,1)

class Network;

/** This class represents a population of neurons. All of the neurons share some characteristics and parameters, so these are included in the Population class.*/
class Population
{
public:
	/** Population constructor. Initialize every variable to 0. */
	Population();
	/** Population desctuctor. If cells were created, delete them. */
	~Population();

	/** Load population instance reading parameters from input stream. 
		This function parses the file. When a connection is found, this function calls CreateConenction(). */
	int loadPopulation(const Network &net, std::istream &in);
	/** Function that retrieves receptor's code for a given label. Called ONLY from parsing functions. */
	int ReceptorCode(const string& s);

	/// Population name.
	string Label;

	/// Number of neurons in this population.
	short Ncells;
	/// Neurons pointer in this population. This neurons are created in the loadPopulation() function.
	Neuron *Cell;

	/// Table of spikes emitted by the neurons of this population
	short TableofSpikes[MAXDELAYINDT][MAXSPIKESINDT];
	/// Index to the current slot in the table of spikes.
	short CTableofSpikes; // pointer where we are (time t)
	/// Index to the delayed (t-delay) slot in the table of spikes.
	short DTableofSpikes; // pointer to the t-delay
	/// Number of spikes saved in each row of the table of spikes.
	short NTableofSpikes[MAXDELAYINDT];

	/// This boolean varible identifies dummy populations.
	bool Dummy; // 1 if population is dummy, 0 otherwise. Neurons belonging to dummy populations 
	//spike at a poisson rate specified by DummyOutputRate
	/// In the case of dummy populations, this is the output rate.
	float DummyOutputRate; 

	/// This function returns the mean frequency between events. This is used for decente output and check what is happening.
	float getMeanFreqBetweenEvents();

	//bool openSpikesFile(const string& fileName);
	//void closeSpikesFile();
	
	/** This function saves all spikes to the output stream. \param spikesFile is the output stream. \param popID is the population ID in the Network population array. \param Time is the current time, for timestamp purposes. */
	void saveAllSpikes(ostream &spikesFile, const int popID, const float Time);
	/** This function traces de V variable of some of the neurons. As this is too much information, not all of the traces are saved. \param tracesFile is the output stream. \param numberOfTraces is number of traces to be saved. */
	void saveTracesV(ostream &tracesFile, const int numberOfTraces);

	//ofstream spikesFile;
	/// Variable to calculate mean frequencies.
	float meanFreqs;
	/// Variable to calculate mean frequencies, but in this case between events.
	float meanFreqsBetweenEvents;
	/** This function takes care of updating frequencies variables: meanFreqs and meanFreqsBetweenEvents. */ 
	void updateFreqs(const float dt);

	/// Capacitance in nF 
	float C; 
	/// (1.0 / C) for computational efficency. Its faster to multiply than to divide.
	float OneOverC; 
	/// Membrane time constant 
	float Taum; 
	/// (1/Taum) for computational efficency. Its faster to multiply than to divide.
	float OneOverTaum; 
	/// Resting potential
	float RestPot; 
	/// Reset potential
	float ResetPot; 
	/// Threshold to determine spike.
	float Threshold;
	/// Refratary period counter.
	float RefPeriod; 

	/// Boolean code indicating if SFA is present.
	bool SFACode;
	/// Calcium concentration. Used only when SFA is present. 
	float dCA; 
	/// Decay time constant of intracellular calcium concentration.
	float tca;
	/// (exp(-dt/tca)) for computational efficency. Exponential is really slow!.
	float ExpConstantTca; //for computational efficency 
	/// Potasium conductancy. Note that gAHP*CA is the efective conductancy.
	float gAHP; 
	/// Potasium reversion potential.
	float Vk;

	/// Connection between two populations.	
	vector<Connection> conn; 

	/// Number of receptors available in this population.
	char Nreceptors;
	/// Receptors array.
	Receptor receptor[MAX_RECEPTORS];

	/** Function used for debugging application. It tries to estimate the amount of memory needed. It sub-estimates a lot, so don't trust it. \return Returns a long with the estimated memory needed in bytes. */
	long calculateMemory();

private:
	/** Load connection from this population's neuron to target neurons, reading parameters from input stream.
		This function parses the file. It is called from loadPopulation(). */
	int CreateConnection(const Network &net, istream &in, const int currentTarget);

	/// After reading population parameters, reserve memory for neurons.
	int reserveMemory();

};

typedef Population* PopulationPtr;

#pragma pack(pop)

#endif /*POPULATION_H_*/
