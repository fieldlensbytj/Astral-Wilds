// Astral Wilds - automation tests for UAstralResonanceWeaveComponent's
// dynamic multicast delegates (OnWeaveResult/OnStabilityChanged), the one
// gap AstralResonanceWeaveComponentTests.cpp explicitly left uncovered
// (see its header comment). Uses UAstralWeaveResultListener - a minimal
// UObject - as the UFUNCTION-bearing bind target dynamic delegates require.
//
// Still NOT covered here: OnPulse, and anything else that only fires from
// the protected TickComponent() (resistance pulses, Resonance Point
// movement, Hold-based Stability gain/decay, the Succeeded-via-Tick path).
// Those genuinely need a real automation test world so Tick can run - a
// bigger lift than fits this pass. What IS covered below fires synchronously
// from public, non-Tick entry points, so it's still a simple automation
// test with no level or Play-in-Editor session required.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralResonanceWeaveComponent.h"
#include "AstralWeaveResultListener.h"

namespace AstralResonanceWeaveComponentDelegateTests
{
	FAstralWeaveTemperament MakeTemperament()
	{
		FAstralWeaveTemperament Temperament;
		Temperament.Volatility = 0.35f;
		Temperament.PulseInterval = 1.6f;
		Temperament.ResistanceStrength = 0.4f;
		Temperament.RequiredStability = 100.f;
		Temperament.bMayFleeOnFailure = true;
		return Temperament;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_BeginWeaveBroadcastsStabilityChanged, "AstralWilds.ResonanceWeave.Delegates.BeginWeaveBroadcastsStabilityChanged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_BeginWeaveBroadcastsStabilityChanged::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	UAstralWeaveResultListener* Listener = NewObject<UAstralWeaveResultListener>();
	Weave->OnStabilityChanged.AddDynamic(Listener, &UAstralWeaveResultListener::HandleStabilityChanged);

	Weave->BeginWeave(AstralResonanceWeaveComponentDelegateTests::MakeTemperament(), false);

	TestTrue(TEXT("BeginWeave broadcasts OnStabilityChanged at least once"), Listener->StabilityChangedCallCount >= 1);
	TestEqual(TEXT("The broadcast stability fraction is zero at the start of a fresh weave"), Listener->LastStabilityFraction, 0.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_ReenteringActiveWeaveBroadcastsMayRetry, "AstralWilds.ResonanceWeave.Delegates.ReenteringActiveWeaveBroadcastsMayRetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_ReenteringActiveWeaveBroadcastsMayRetry::RunTest(const FString& Parameters)
{
	// BeginWeave() while a weave is already active is documented to reject
	// the re-entry and broadcast MayRetry rather than resetting the running
	// attempt - this is reachable synchronously with no Tick required.
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	UAstralWeaveResultListener* Listener = NewObject<UAstralWeaveResultListener>();
	Weave->OnWeaveResult.AddDynamic(Listener, &UAstralWeaveResultListener::HandleWeaveResult);

	Weave->BeginWeave(AstralResonanceWeaveComponentDelegateTests::MakeTemperament(), false);
	TestFalse(TEXT("The first BeginWeave call does not resolve the weave"), Listener->bReceivedWeaveResult);

	Weave->BeginWeave(AstralResonanceWeaveComponentDelegateTests::MakeTemperament(), false);

	TestTrue(TEXT("Re-entering an active weave broadcasts a result"), Listener->bReceivedWeaveResult);
	TestEqual(TEXT("Re-entering an active weave broadcasts MayRetry specifically"), (uint8)Listener->LastWeaveResult, (uint8)EAstralWeaveResult::MayRetry);
	TestEqual(TEXT("Re-entering an active weave broadcasts exactly once"), Listener->WeaveResultCallCount, 1);
	TestTrue(TEXT("The original weave attempt is still active after the rejected re-entry"), Weave->IsWeaveActive());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_CancelWeaveDoesNotBroadcastAResult, "AstralWilds.ResonanceWeave.Delegates.CancelWeaveDoesNotBroadcastAResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_CancelWeaveDoesNotBroadcastAResult::RunTest(const FString& Parameters)
{
	// CancelWeave is documented as ending the attempt "without resolving
	// success or failure" - unlike every other way a weave ends, it should
	// NOT broadcast OnWeaveResult at all.
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	UAstralWeaveResultListener* Listener = NewObject<UAstralWeaveResultListener>();
	Weave->OnWeaveResult.AddDynamic(Listener, &UAstralWeaveResultListener::HandleWeaveResult);

	Weave->BeginWeave(AstralResonanceWeaveComponentDelegateTests::MakeTemperament(), false);
	Weave->CancelWeave();

	TestFalse(TEXT("CancelWeave does not broadcast a weave result"), Listener->bReceivedWeaveResult);
	TestFalse(TEXT("The weave is no longer active after cancelling"), Weave->IsWeaveActive());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
