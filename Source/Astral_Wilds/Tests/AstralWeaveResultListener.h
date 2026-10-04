// Astral Wilds - a minimal UObject listener used only by automation tests to
// observe UAstralResonanceWeaveComponent's dynamic multicast delegates
// (OnWeaveResult/OnStabilityChanged/OnPulse). UE's DECLARE_DYNAMIC_MULTICAST_
// DELEGATE requires a UFUNCTION-bearing UObject target for AddDynamic(), so
// a plain FAutomationTestBase (not itself a UObject) can't bind directly -
// this class exists purely to give a test something concrete to bind to and
// read back afterward. Not used anywhere outside Tests/.
#pragma once

#include "CoreMinimal.h"
#include "AstralTypes.h"
#include "AstralWeaveResultListener.generated.h"

UCLASS()
class UAstralWeaveResultListener : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION()
	void HandleWeaveResult(EAstralWeaveResult Result);

	UFUNCTION()
	void HandleStabilityChanged(float StabilityFraction);

	UFUNCTION()
	void HandlePulse(float PulseResponseWindowSeconds);

	/** True once OnWeaveResult has fired at least once. */
	bool bReceivedWeaveResult = false;
	EAstralWeaveResult LastWeaveResult = EAstralWeaveResult::InProgress;
	int32 WeaveResultCallCount = 0;

	int32 StabilityChangedCallCount = 0;
	float LastStabilityFraction = 0.f;

	int32 PulseCallCount = 0;
	float LastPulseWindow = 0.f;
};
