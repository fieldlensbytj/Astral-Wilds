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
	for (EAstralAIArchetype Arch : { EAstralAIArchetype::Aggressive, EAstralAIArchetype::Skittish, EAstralAIArchetype::Territorial, EAstralAIArchetype::Docile })
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
	TestEqual(TEXT("Idles at home while the player stays out of its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Idle, 400.f, 0.f, 600.f), EMode::Idle);
	TestEqual(TEXT("Chases a player inside its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Idle, 300.f, 0.f, 300.f), EMode::Chase);
	TestEqual(TEXT("Returns home once the player leaves its territory"), Choose(EAstralAIArchetype::Territorial, EMode::Chase, 900.f, 450.f, 800.f), EMode::ReturnHome);
	TestEqual(TEXT("Idles again once home"), Choose(EAstralAIArchetype::Territorial, EMode::ReturnHome, 900.f, 150.f, 800.f), EMode::Idle);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
