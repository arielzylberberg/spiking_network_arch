#ifndef SYNAPSE_H_
#define SYNAPSE_H_

#include "Connection.h"
#include "Receptor.h"

#pragma pack(push,1)

class Synapse
{
public:
	Synapse();
	virtual ~Synapse()=0;

	short int connectionId;
	short int TargetNeuron;

	float EfficacyMod; //factor that multiplies "Efficacy"

	virtual void decayPlasticity(double deltaTime, const Connection & conn) = 0;
	virtual void updatePlasticity(const Connection & conn) = 0;

	virtual float getLSsumand() = 0;

	// Only for NMDA axonals.
	virtual void updateConductance() = 0;
	virtual void decayConductance(float deltaTime, const Receptor &receptor) = 0;

	virtual long calculateMemory() = 0;

};

typedef Synapse* SynapsePtr;

#pragma pack(pop)

#endif /*SYNAPSE_H_*/
