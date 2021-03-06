#ifndef __EVENT_H
#define __EVENT_H

#include <string>
#include <vector>

#include "constants.h"
#include "Network.h"

using namespace std;

/** This class represents a event. This class should be an abstract class, generic for any event. 
As events are very few and simple, these where implemented together in this class. */
class Event
{
public:
	/** Event constructor. Nothing to be done. */
	Event(void);
	/** Event destructor. Nothing to be done. */
	~Event(void);

	/** Type of event. */
	int Type;
	/** Time of event. */
	float ETime;
	/** Population number affected by event. */
	int PopNumber;
	/** Receptor involved in event. */
	int ReceptorNumber;
	/** External frequency for input signals events. */
	float FreqExt;
	/** If this event represent dummy input to the population, this is the parameter to use. */
	float DummyOutput; 
	/** Event label. */
	string Label;

	/** For Gaussian inputs. Standard deviation of the input, normalized to the range 0-1 */
	float StdDevExt;
	
	/** For event-types that modify only specific cells whithin the population */
	vector<int> ETargetCells;

	/** This function handles the event. \param network is received to alter values. */
	void handleEvent(Network&);

	/** This function compares two events, necessary in order to use std library function sort. */
	static bool compareEvents(const Event* ev1, const Event* ev2) {return (ev1->ETime <ev2->ETime);}
};

#endif

