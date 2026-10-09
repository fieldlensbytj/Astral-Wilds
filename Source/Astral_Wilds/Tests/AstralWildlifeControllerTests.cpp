// Astral Wilds - automation tests for AAstralWildlifeController::ChooseMode,
// the pure decision rule behind the native wild Astral behavior. Movement
// itself needs a navmesh and is exercised by Astral.AutoPlaytest instead.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralWildlifeController.h"

namespace AstralWildlifeControllerTests
{
	using EMode = EAstralWildlifeMode;
	const FAstralWildlifeTuning T; // defaults: alert 600, calm 1000, give up 1200, max chase from home 1500, territory 500, home 200

	EMode Choose(EAstralAIArchetype Arch, EMode Current, float DistToPlayer, float DistFromHome = 0.f, float PlayerDistFromHome = 5000.f,
		EAstralWildState State = EAstralWildState::Calm, bool bFleeStalled = false, bool bRecentlyCornered = false)
	{
		return AAstralWildlifeController::ChooseMode(Arch, State, Current, DistToPlayer, DistFromHome, PlayerDistFromHome, T, bFleeStalled, bRecentlyCornered);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_ReceptiveHoldsStill, "AstralWilds.Wildlife.ReceptiveAlwaysHoldsStill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_ReceptiveHoldsStill::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	for (EAstralAIArchetype Arch : { EAstralAIArchetype::Aggressive, EAstralAIArchetype::Skittish, EAstralAIArchetype::Territorial, EAstralAIArchetype::Docile, EAstralAIArchetype::Wary })
	{
		TestEqual(*FString::Printf(TEXT("Receptive %s idles with the player close"), *UEnum::GetValueAsString(Arch)),
			Choose(Arch, EMode::Wander, 100.f, 0.f, 100.f, EAstralWildState::Receptive), EMode::Idle);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_DocileWanders, "AstralWilds.Wildlife.DocileAlwaysWanders", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_DocileWanders::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	TestEqual(TEXT("Docile ignores a nearby player"), Choose(EAstralAIArchetype::Docile, EMode::Idle, 100.f), EMode::Wander);
	TestEqual(TEXT("Docile wanders with nobody around"), Choose(EAstralAIArchetype::Docile, EMode::Idle, 5000.f), EMode::Wander);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_SkittishFleesWithHysteresis, "AstralWilds.Wildlife.SkittishFleesUntilCalmRange", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_SkittishFleesWithHysteresis::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	TestEqual(TEXT("Wanders while the player is outside alert range"), Choose(EAstralAIArchetype::Skittish, EMode::Wander, 700.f), EMode::Wander);
	TestEqual(TEXT("Flees once the player is inside alert range"), Choose(EAstralAIArchetype::Skittish, EMode::Wander, 500.f), EMode::Flee);
	TestEqual(TEXT("Keeps fleeing between alert and calm range"), Choose(EAstralAIArchetype::Skittish, EMode::Flee, 800.f), EMode::Flee);
	TestEqual(TEXT("Calms down beyond calm range"), Choose(EAstralAIArchetype::Skittish, EMode::Flee, 1100.f), EMode::Wander);
	TestEqual(TEXT("Stalled flee: settles instead of fleeing in place"), Choose(EAstralAIArchetype::Skittish, EMode::Flee, 520.f, 0.f, 5000.f, EAstralWildState::Calm, true, true), EMode::Wander);
	TestEqual(TEXT("Recently cornered: a player at 5.2m doesn't re-spook it (the 2026-10-05 loop)"), Choose(EAstralAIArchetype::Skittish, EMode::Wander, 520.f, 0.f, 5000.f, EAstralWildState::Calm, false, true), EMode::Wander);
	TestEqual(TEXT("Recently cornered: a player inside 3m still does"), Choose(EAstralAIArchetype::Skittish, EMode::Wander, 250.f, 0.f, 5000.f, EAstralWildState::Calm, false, true), EMode::Flee);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_WaryBacksOffWhenClose, "AstralWilds.Wildlife.WaryBacksOffOnlyWhenClose", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_WaryBacksOffWhenClose::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	TestEqual(TEXT("Lets the player within 5m (Skittish bolts at 6m) and keeps wandering"), Choose(EAstralAIArchetype::Wary, EMode::Wander, 500.f), EMode::Wander);
	TestEqual(TEXT("Backs off once the player is within 3m"), Choose(EAstralAIArchetype::Wary, EMode::Wander, 300.f), EMode::Flee);
	TestEqual(TEXT("Keeps backing off until it has some room"), Choose(EAstralAIArchetype::Wary, EMode::Flee, 600.f), EMode::Flee);
	TestEqual(TEXT("Settles at 7.5m, sooner than Skittish (10m)"), Choose(EAstralAIArchetype::Wary, EMode::Flee, 750.f), EMode::Wander);
	TestEqual(TEXT("Recently cornered: a player at 3m doesn't re-spook it"), Choose(EAstralAIArchetype::Wary, EMode::Wander, 300.f, 0.f, 5000.f, EAstralWildState::Calm, false, true), EMode::Wander);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_AggressiveChasesWithLeash, "AstralWilds.Wildlife.AggressiveChasesWithinLeash", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_AggressiveChasesWithLeash::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	TestEqual(TEXT("Wanders while the player is outside alert range"), Choose(EAstralAIArchetype::Aggressive, EMode::Wander, 700.f), EMode::Wander);
	TestEqual(TEXT("Chases once the player is inside alert range"), Choose(EAstralAIArchetype::Aggressive, EMode::Wander, 500.f), EMode::Chase);
	TestEqual(TEXT("Keeps chasing up to give-up range"), Choose(EAstralAIArchetype::Aggressive, EMode::Chase, 1100.f, 600.f), EMode::Chase);
	TestEqual(TEXT("Gives up past give-up range and heads home"), Choose(EAstralAIArchetype::Aggressive, EMode::Chase, 1300.f, 600.f), EMode::ReturnHome);
	TestEqual(TEXT("Gives up when too far from home, even with the player close"), Choose(EAstralAIArchetype::Aggressive, EMode::Chase, 300.f, 1600.f), EMode::ReturnHome);
	TestEqual(TEXT("Keeps returning until home"), Choose(EAstralAIArchetype::Aggressive, EMode::ReturnHome, 3000.f, 400.f), EMode::ReturnHome);
	TestEqual(TEXT("Back to wandering once home"), Choose(EAstralAIArchetype::Aggressive, EMode::ReturnHome, 3000.f, 100.f), EMode::Wander);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_TerritorialGuardsHome, "AstralWilds.Wildlife.TerritorialGuardsHome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_TerritorialGuardsHome::RunTest(const FString& Parameters)
{
	using namespace AstralWildlifeControllerTests;
	TestEqual(TEXT("Patrols while the player stays out of its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Idle, 400.f, 0.f, 600.f), EMode::Wander);
	TestEqual(TEXT("Keeps patrolling away from home (no pull back)"), Choose(EAstralAIArchetype::Territorial, EMode::Wander, 900.f, 600.f, 800.f), EMode::Wander);
	TestEqual(TEXT("Chases a player inside its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Wander, 300.f, 0.f, 300.f), EMode::Chase);
	TestEqual(TEXT("Returns home once the player leaves its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Chase, 900.f, 450.f, 800.f), EMode::ReturnHome);
	TestEqual(TEXT("Patrols again once home"), Choose(EAstralAIArchetype::Territorial, EMode::ReturnHome, 900.f, 150.f, 800.f), EMode::Wander);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_QuarryScore, "AstralWilds.Wildlife.FlyerPicksMovingQuarry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_QuarryScore::RunTest(const FString& Parameters)
{
	const float Range = 3000.f;
	TestEqual(TEXT("Nothing out of range"), AAstralWildlifeController::QuarryScore(3100.f, Range, true, true), 0.f);
	TestTrue(TEXT("Something in range is worth watching"), AAstralWildlifeController::QuarryScore(2900.f, Range, false, false) > 0.f);
	TestTrue(TEXT("A moving Astral beats a still one a little nearer"),
		AAstralWildlifeController::QuarryScore(1500.f, Range, true, false) > AAstralWildlifeController::QuarryScore(1000.f, Range, false, false));
	TestTrue(TEXT("Nearer beats farther, all else equal"),
		AAstralWildlifeController::QuarryScore(800.f, Range, false, false) > AAstralWildlifeController::QuarryScore(2000.f, Range, false, false));
	TestTrue(TEXT("The Mage edges out an Astral at the same distance"),
		AAstralWildlifeController::QuarryScore(1000.f, Range, true, true) > AAstralWildlifeController::QuarryScore(1000.f, Range, true, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildlife_MeanderTarget, "AstralWilds.Wildlife.MeanderCurvesOnAndHome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildlife_MeanderTarget::RunTest(const FString& Parameters)
{
	// Near home, facing +X: it walks on ahead, turned by Turn x 70 deg.
	const FVector Home = FVector::ZeroVector;
	const FVector Ahead = AAstralWildlifeController::MeanderTarget(FVector(100.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), Home, 800.f, 0.f, 500.f);
	TestTrue(TEXT("No turn: straight on"), Ahead.Equals(FVector(600.f, 0.f, 0.f), 0.1f));
	const FVector Turned = AAstralWildlifeController::MeanderTarget(FVector(100.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), Home, 800.f, 0.5f, 500.f);
	const float TurnDeg = FMath::RadiansToDegrees(FMath::Atan2(Turned.Y, Turned.X - 100.f));
	TestTrue(TEXT("Half a turn: 35 deg off its facing"), FMath::IsNearlyEqual(FMath::Abs(TurnDeg), 35.f, 0.5f));
	TestTrue(TEXT("The distance is kept"), FMath::IsNearlyEqual(FVector::Dist(Turned, FVector(100.f, 0.f, 0.f)), 500.f, 0.5f));

	// At the edge of its patch, still facing out: it heads back toward home.
	const FVector Edge(800.f, 0.f, 0.f);
	const FVector Back = AAstralWildlifeController::MeanderTarget(Edge, FVector(1.f, 0.f, 0.f), Home, 800.f, 0.2f, 500.f);
	TestTrue(TEXT("At the edge it turns for home"), FVector::Dist2D(Back, Home) < FVector::Dist2D(Edge, Home));
	// Facing straight away from home at the edge (the lerp would cancel): still home.
	const FVector Away = AAstralWildlifeController::MeanderTarget(Edge, FVector(1.f, 0.f, 0.f), Home, 800.f, 0.f, 500.f);
	TestTrue(TEXT("Facing dead away, it still comes back"), Away.X < Edge.X);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
