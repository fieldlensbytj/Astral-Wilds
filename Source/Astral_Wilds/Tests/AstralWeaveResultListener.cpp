#include "AstralWeaveResultListener.h"

void UAstralWeaveResultListener::HandleWeaveResult(EAstralWeaveResult Result)
{
	bReceivedWeaveResult = true;
	LastWeaveResult = Result;
	WeaveResultCallCount++;
}

void UAstralWeaveResultListener::HandleStabilityChanged(float StabilityFraction)
{
	LastStabilityFraction = StabilityFraction;
	StabilityChangedCallCount++;
}

void UAstralWeaveResultListener::HandlePulse(float PulseResponseWindowSeconds)
{
	LastPulseWindow = PulseResponseWindowSeconds;
	PulseCallCount++;
}
