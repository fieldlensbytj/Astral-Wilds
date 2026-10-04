// Astral Wilds - automation tests for UAstralResonanceWeaveComponent's
// Tick-driven surface, the gap AstralResonanceWeaveComponentTests.cpp and
// AstralResonanceWeaveComponentDelegateTests.cpp both left open: Resonance
// Point movement, pulse timing, Hold-based Stability gain/decay, Harmonize
// scoring inside a real pulse window, and the Succeeded-via-Tick path.
//
// UActorComponent::TickComponent check()s that the component is registered,
// so each test builds a throwaway UWorld, spawns a bare owner actor and
// registers the component on it (FWeaveTickHarness below). Tick is then
// driven by hand through the public base-class TickComponent - the world
// itself never ticks, so timing is fully deterministic. Volatility 0 keeps
// the Resonance Point's random jitter at exactly zero.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralResonanceWeaveComponent.h"
#include "AstralWeaveResultListener.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace AstralResonanceWeaveComponentTickTests
{
	FAstralWeaveTemperament MakeTemperament(float PulseInterval = 100.f, float RequiredStability = 100.f)
	{
		FAstralWeaveTemperament Temperament;
		Temperament.Volatility = 0.f;
		Temperament.PulseInterval = PulseInterval;
		Temperament.ResistanceStrength = 0.4f;
		Temperament.RequiredStability = RequiredStability;
		Temperament.bMayFleeOnFailure = true;
		return Temperament;
	}

	struct FWeaveTickHarness
	{
		UWorld* World = nullptr;
		UAstralResonanceWeaveComponent* Weave = nullptr;
		UAstralWeaveResultListener* Listener = nullptr;

		FWeaveTickHarness()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AstralWeaveTickTestWorld"));
			AActor* Owner = World->SpawnActor<AActor>();
			Weave = NewObject<UAstralResonanceWeaveComponent>(Owner);
			Weave->RegisterComponent();

			Listener = NewObject<UAstralWeaveResultListener>();
			Weave->OnWeaveResult.AddDynamic(Listener, &UAstralWeaveResultListener::HandleWeaveResult);
			Weave->OnStabilityChanged.AddDynamic(Listener, &UAstralWeaveResultListener::HandleStabilityChanged);
			Weave->OnPulse.AddDynamic(Listener, &UAstralWeaveResultListener::HandlePulse);
		}

		~FWeaveTickHarness()
		{
			World->DestroyWorld(false);
		}

		/** Snaps the reticle onto the Resonance Point (perfect tracking). */
		void TrackPoint()
		{
			Weave->SetAlignmentInput(Weave->GetResonancePoint() - Weave->GetAlignmentReticle());
		}

		/** Moves the reticle to the opposite side of the Sigil from the Resonance Point (well outside tolerance). */
		void LoseAlignment()
		{
			Weave->SetAlignmentInput(-Weave->GetResonancePoint().GetSafeNormal() - Weave->GetAlignmentReticle());
		}

		/** Ticks Steps times at Dt, optionally re-tracking the point before each tick. Stops early once the weave resolves. */
		void Tick(float Dt, int32 Steps, bool bTrack = false)
		{
			for (int32 i = 0; i < Steps && Weave->IsWeaveActive(); ++i)
			{
				if (bTrack)
				{
					TrackPoint();
				}
				static_cast<UActorComponent*>(Weave)->TickComponent(Dt, LEVELTICK_All, nullptr);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_ResonancePointMoves, "AstralWilds.ResonanceWeave.Tick.ResonancePointMovesWithinSigil", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_ResonancePointMoves::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(), false);

	H.Tick(0.1f, 1);
	const FVector2D First = H.Weave->GetResonancePoint();
	H.Tick(0.1f, 20);
	const FVector2D Later = H.Weave->GetResonancePoint();

	TestFalse(TEXT("Resonance Point moves over time"), First.Equals(Later, KINDA_SMALL_NUMBER));
	// Volatility 0 -> amplitude 0.4 on each axis, no jitter.
	TestTrue(TEXT("Calm Astral's point stays within its 0.4 amplitude"), FMath::Abs(Later.X) <= 0.4f + KINDA_SMALL_NUMBER && FMath::Abs(Later.Y) <= 0.4f + KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_PulseFiresAfterInterval, "AstralWilds.ResonanceWeave.Tick.PulseFiresAfterInterval", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_PulseFiresAfterInterval::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(1.0f), false);

	H.Tick(0.1f, 9);
	TestEqual(TEXT("No pulse before the interval elapses"), H.Listener->PulseCallCount, 0);

	H.Tick(0.1f, 2);
	TestEqual(TEXT("Exactly one pulse once the interval elapses"), H.Listener->PulseCallCount, 1);
	TestEqual(TEXT("Pulse reports the 0.45s Harmonize response window"), H.Listener->LastPulseWindow, 0.45f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_ChannelingAlignedGainsStability, "AstralWilds.ResonanceWeave.Tick.ChannelingAlignedGainsStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_ChannelingAlignedGainsStability::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(), false);
	H.Weave->SetChanneling(true);

	H.Tick(0.01f, 100, /*bTrack*/ true);

	// Perfect tracking -> quality ~1 -> ~18 Stability/sec out of 100.
	TestTrue(TEXT("One second of aligned channeling builds ~18% Stability"), H.Weave->GetStabilityFraction() > 0.15f && H.Weave->GetStabilityFraction() < 0.19f);
	TestTrue(TEXT("Weave still in progress"), H.Weave->IsWeaveActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_ChannelingMisalignedDecays, "AstralWilds.ResonanceWeave.Tick.ChannelingMisalignedDecays", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_ChannelingMisalignedDecays::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(), false);
	H.Weave->SetChanneling(true);
	H.Tick(0.01f, 100, /*bTrack*/ true);
	const float Built = H.Weave->GetStabilityFraction();

	H.LoseAlignment();
	H.Tick(0.01f, 50);

	// 6 * (1 + 0.4) = 8.4 Stability/sec lost -> ~4.2 over half a second.
	TestTrue(TEXT("Misaligned channeling drains Stability"), H.Weave->GetStabilityFraction() < Built);
	TestEqual(TEXT("Drain rate matches 6 * (1 + ResistanceStrength) per second"), Built - H.Weave->GetStabilityFraction(), 0.042f, 0.002f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_ReleasingHoldDoesNotDecay, "AstralWilds.ResonanceWeave.Tick.ReleasingHoldDoesNotDecay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_ReleasingHoldDoesNotDecay::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(), false);
	H.Weave->SetChanneling(true);
	H.Tick(0.01f, 100, /*bTrack*/ true);
	const float Built = H.Weave->GetStabilityFraction();

	H.Weave->SetChanneling(false);
	H.LoseAlignment();
	H.Tick(0.01f, 100);

	TestEqual(TEXT("Not channeling holds Stability steady, even when misaligned"), H.Weave->GetStabilityFraction(), Built, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_SucceedsAtRequiredStability, "AstralWilds.ResonanceWeave.Tick.SucceedsAtRequiredStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_SucceedsAtRequiredStability::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(100.f, 5.f), false);
	H.Weave->SetChanneling(true);

	H.Tick(0.01f, 200, /*bTrack*/ true);

	TestTrue(TEXT("OnWeaveResult fired"), H.Listener->bReceivedWeaveResult);
	TestEqual(TEXT("Result is Succeeded"), H.Listener->LastWeaveResult, EAstralWeaveResult::Succeeded);
	TestEqual(TEXT("Result fired exactly once"), H.Listener->WeaveResultCallCount, 1);
	TestFalse(TEXT("Weave is no longer active"), H.Weave->IsWeaveActive());
	TestEqual(TEXT("Stability ends full"), H.Weave->GetStabilityFraction(), 1.f, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_MissedPulseAtZeroStability, "AstralWilds.ResonanceWeave.Tick.MissedPulseAtZeroStabilityFailsWeave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_MissedPulseAtZeroStability::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	{
		FWeaveTickHarness H;
		H.Weave->BeginWeave(MakeTemperament(0.4f), false);
		H.Tick(0.05f, 30);
		TestEqual(TEXT("A pulse fired"), H.Listener->PulseCallCount, 1);
		TestEqual(TEXT("Missing it at zero Stability makes a flighty Astral flee"), H.Listener->LastWeaveResult, EAstralWeaveResult::Fled);
		TestFalse(TEXT("Weave is no longer active"), H.Weave->IsWeaveActive());
	}
	{
		FWeaveTickHarness H;
		FAstralWeaveTemperament Stubborn = MakeTemperament(0.4f);
		Stubborn.bMayFleeOnFailure = false;
		H.Weave->BeginWeave(Stubborn, false);
		H.Tick(0.05f, 30);
		TestEqual(TEXT("A non-fleeing Astral allows a retry instead"), H.Listener->LastWeaveResult, EAstralWeaveResult::MayRetry);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_MissedPulseDrainsStability, "AstralWilds.ResonanceWeave.Tick.MissedPulseDrainsStability", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_MissedPulseDrainsStability::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	// Build Stability with a long pulse interval, then let the first pulse fire and lapse without channeling.
	H.Weave->BeginWeave(MakeTemperament(2.0f), false);
	H.Weave->SetChanneling(true);
	H.Tick(0.01f, 150, /*bTrack*/ true);
	H.Weave->SetChanneling(false);
	const float Built = H.Weave->GetStabilityFraction();

	H.Tick(0.01f, 50 + 60); // reach the 2.0s pulse, then outlast its 0.45s window (next pulse is at 4.0s)
	TestEqual(TEXT("A pulse fired"), H.Listener->PulseCallCount, 1);
	// 12 * (1 + 0.4) = 16.8 Stability lost for the miss.
	TestEqual(TEXT("Missed pulse costs 12 * (1 + ResistanceStrength)"), Built - H.Weave->GetStabilityFraction(), 0.168f, 0.002f);
	TestTrue(TEXT("Weave survives the miss with Stability to spare"), H.Weave->IsWeaveActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_HarmonizeInWindow, "AstralWilds.ResonanceWeave.Tick.HarmonizeInWindowAlignedGains", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_HarmonizeInWindow::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	FWeaveTickHarness H;
	H.Weave->BeginWeave(MakeTemperament(0.4f), false);
	H.Tick(0.01f, 41, /*bTrack*/ true);
	TestEqual(TEXT("Pulse is live"), H.Listener->PulseCallCount, 1);

	H.TrackPoint();
	H.Weave->RespondToHarmonize();

	// 15 + 10 * quality, quality ~1 with perfect tracking.
	TestTrue(TEXT("Aligned Harmonize in the window grants ~25 Stability"), H.Weave->GetStabilityFraction() > 0.24f && H.Weave->GetStabilityFraction() <= 0.25f + KINDA_SMALL_NUMBER);

	const float AfterFirst = H.Weave->GetStabilityFraction();
	H.Weave->RespondToHarmonize();
	TestEqual(TEXT("A second press on the same pulse is ignored"), H.Weave->GetStabilityFraction(), AfterFirst, KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralResonanceWeave_Tick_HarmonizeMisaligned, "AstralWilds.ResonanceWeave.Tick.HarmonizeMisalignedOnlyForgivenByOldConcordance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralResonanceWeave_Tick_HarmonizeMisaligned::RunTest(const FString& Parameters)
{
	using namespace AstralResonanceWeaveComponentTickTests;
	{
		FWeaveTickHarness H;
		H.Weave->BeginWeave(MakeTemperament(0.4f), false);
		H.Tick(0.01f, 41);
		H.LoseAlignment();
		H.Weave->RespondToHarmonize();
		TestEqual(TEXT("Misaligned Harmonize at zero Stability fails the weave"), H.Listener->LastWeaveResult, EAstralWeaveResult::Fled);
	}
	{
		FWeaveTickHarness H;
		H.Weave->BeginWeave(MakeTemperament(0.4f), /*bUseOldConcordance*/ true);
		H.Tick(0.01f, 41);
		H.LoseAlignment();
		H.Weave->RespondToHarmonize();
		TestTrue(TEXT("Old Concordance forgives misalignment: still gains the base 15"), FMath::IsNearlyEqual(H.Weave->GetStabilityFraction(), 0.15f, 0.001f));
		TestTrue(TEXT("Weave continues"), H.Weave->IsWeaveActive());
	}
	return true;
}

#endif // WITH_AUTOMATION_TESTS
