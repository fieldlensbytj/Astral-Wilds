// Astral Wilds - automation tests for UAstralResonanceWeaveComponent, the
// Track/Hold/Harmonize bonding minigame (Canon Bible Section VII). This
// covers only the public, non-Tick-driven surface: BeginWeave/CancelWeave
// state, SetAlignmentInput's unit-circle clamping, and RespondToHarmonize's
// documented no-op behavior outside an active pulse window. None of these
// call GetWorld() or GetOwner(), so - like the other Tests/ files - the
// component is constructed with a bare NewObject<>(), no actor/level needed.
//
// NOT covered here (needs a real automation test world so TickComponent -
// currently protected - can actually run): Resonance Point movement,
// pulse-window timing, Hold-based Stability gain/decay, and the
// OnWeaveResult/OnStabilityChanged/OnPulse delegate broadcasts themselves
// (UE's DECLARE_DYNAMIC_MULTICAST_DELEGATE requires a UFUNCTION-bearing
// listener object to bind to, which is more setup than fits this pass).
// Good next-session candidate - see the dated review note.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralResonanceWeaveComponent.h"

namespace AstralResonanceWeaveComponentTests
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_DefaultState, "AstralWilds.ResonanceWeave.DefaultState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_DefaultState::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	TestFalse(TEXT("A freshly constructed component has no weave in progress"), Weave->IsWeaveActive());
	TestEqual(TEXT("Stability fraction starts at zero"), Weave->GetStabilityFraction(), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_BeginWeaveActivates, "AstralWilds.ResonanceWeave.BeginWeave.Activates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_BeginWeaveActivates::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	Weave->BeginWeave(AstralResonanceWeaveComponentTests::MakeTemperament(), false);

	TestTrue(TEXT("BeginWeave activates the weave"), Weave->IsWeaveActive());
	TestEqual(TEXT("Stability fraction starts at zero even once active"), Weave->GetStabilityFraction(), 0.f);
	TestEqual(TEXT("The Resonance Point starts at the Sigil's center before any tick runs"), Weave->GetResonancePoint(), FVector2D::ZeroVector);
	TestEqual(TEXT("The player's alignment reticle starts at the Sigil's center"), Weave->GetAlignmentReticle(), FVector2D::ZeroVector);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_CancelWeave, "AstralWilds.ResonanceWeave.CancelWeave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_CancelWeave::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();

	// Cancelling with nothing in progress is a documented safe no-op.
	Weave->CancelWeave();
	TestFalse(TEXT("Cancelling an inactive weave is a safe no-op"), Weave->IsWeaveActive());

	Weave->BeginWeave(AstralResonanceWeaveComponentTests::MakeTemperament(), false);
	TestTrue(TEXT("Sanity check: the weave is active before cancelling"), Weave->IsWeaveActive());

	Weave->CancelWeave();
	TestFalse(TEXT("CancelWeave deactivates an in-progress weave"), Weave->IsWeaveActive());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_AlignmentInputClamping, "AstralWilds.ResonanceWeave.SetAlignmentInput.Clamping", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_AlignmentInputClamping::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	Weave->BeginWeave(AstralResonanceWeaveComponentTests::MakeTemperament(), false);

	// A small, well-within-bounds delta should accumulate exactly, unclamped.
	Weave->SetAlignmentInput(FVector2D(0.1f, 0.2f));
	const FVector2D SmallResult = Weave->GetAlignmentReticle();
	TestTrue(TEXT("A small delta within the unit circle is not clamped"),
		SmallResult.Equals(FVector2D(0.1f, 0.2f), KINDA_SMALL_NUMBER));

	// A large delta should push the reticle out to, but never past, the unit circle.
	UAstralResonanceWeaveComponent* Weave2 = NewObject<UAstralResonanceWeaveComponent>();
	Weave2->BeginWeave(AstralResonanceWeaveComponentTests::MakeTemperament(), false);
	Weave2->SetAlignmentInput(FVector2D(10.f, 10.f));
	const float LargeResultSize = Weave2->GetAlignmentReticle().Size();
	TestTrue(TEXT("A large delta clamps the reticle to the unit circle (size <= 1)"), LargeResultSize <= 1.f + KINDA_SMALL_NUMBER);
	TestTrue(TEXT("A large delta normalizes the reticle to exactly the unit circle, not partway"),
		FMath::IsNearlyEqual(LargeResultSize, 1.f, KINDA_SMALL_NUMBER));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_AlignmentInputIgnoredWhenInactive, "AstralWilds.ResonanceWeave.SetAlignmentInput.IgnoredWhenInactive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_AlignmentInputIgnoredWhenInactive::RunTest(const FString& Parameters)
{
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	// No BeginWeave() call - the weave is not active.
	Weave->SetAlignmentInput(FVector2D(0.5f, 0.5f));
	TestEqual(TEXT("Alignment input is ignored entirely while no weave is active"), Weave->GetAlignmentReticle(), FVector2D::ZeroVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_HarmonizeOutsidePulseIsNoOp, "AstralWilds.ResonanceWeave.RespondToHarmonize.NoOpOutsidePulse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_HarmonizeOutsidePulseIsNoOp::RunTest(const FString& Parameters)
{
	// Per the component's own doc comment: "Pressing Harmonize with no pulse
	// active is simply ignored - there is no punishment for an eager or
	// mistimed press outside a pulse window." No tick ever runs in this test,
	// so a pulse can never have fired - this exercises exactly that no-op path.
	UAstralResonanceWeaveComponent* Weave = NewObject<UAstralResonanceWeaveComponent>();
	Weave->BeginWeave(AstralResonanceWeaveComponentTests::MakeTemperament(), false);

	Weave->RespondToHarmonize();

	TestTrue(TEXT("The weave stays active after an out-of-window Harmonize press"), Weave->IsWeaveActive());
	TestEqual(TEXT("Stability is unaffected by an out-of-window Harmonize press"), Weave->GetStabilityFraction(), 0.f);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
