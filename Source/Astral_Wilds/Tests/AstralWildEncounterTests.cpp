// Astral Wilds - automation tests for AWildAstralEncounter's receptiveness
// state machine (Canon Bible Section VII, Step 1: "create receptiveness").
// TryBecomeReceptive()/GetWildState()/IsReceptive()/SetWildState() never
// touch GetWorld() or any other world-dependent API, so - like
// UAstralBattleEngine - this is constructed with a bare NewObject<>() and
// tested as pure state-machine logic, no level or Play-in-Editor session
// required.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralWildEncounter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildEncounter_DefaultState, "AstralWilds.WildEncounter.DefaultState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildEncounter_DefaultState::RunTest(const FString& Parameters)
{
	AWildAstralEncounter* Encounter = NewObject<AWildAstralEncounter>();
	TestEqual(TEXT("A freshly constructed wild Astral starts Calm"), (uint8)Encounter->GetWildState(), (uint8)EAstralWildState::Calm);
	TestFalse(TEXT("A Calm Astral is not yet receptive"), Encounter->IsReceptive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildEncounter_NonAggressiveStatesBecomeReceptive, "AstralWilds.WildEncounter.TryBecomeReceptive.NonAggressive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildEncounter_NonAggressiveStatesBecomeReceptive::RunTest(const FString& Parameters)
{
	// Every state the base implementation is allowed to resolve on its own -
	// a Blueprint/C++ subclass is expected to override the aggressive states
	// tested separately below, but these should always work from the base class.
	const EAstralWildState NonAggressiveStates[] = {
		EAstralWildState::Calm,
		EAstralWildState::Curious,
		EAstralWildState::Wary,
		EAstralWildState::Frightened,
	};

	for (EAstralWildState StartState : NonAggressiveStates)
	{
		AWildAstralEncounter* Encounter = NewObject<AWildAstralEncounter>();
		Encounter->SetWildState(StartState);

		Encounter->TryBecomeReceptive();

		TestEqual(*FString::Printf(TEXT("Starting from %d, TryBecomeReceptive() reaches Receptive"), (int32)StartState),
			(uint8)Encounter->GetWildState(), (uint8)EAstralWildState::Receptive);
		TestTrue(*FString::Printf(TEXT("Starting from %d, IsReceptive() is true after TryBecomeReceptive()"), (int32)StartState),
			Encounter->IsReceptive());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildEncounter_AggressiveStatesRefuseToTransition, "AstralWilds.WildEncounter.TryBecomeReceptive.Aggressive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildEncounter_AggressiveStatesRefuseToTransition::RunTest(const FString& Parameters)
{
	// The whole point of the base implementation being narrow: Territorial and
	// Enraged must NOT silently become bondable on their own. A real species
	// behavior (combat/exhaustion per the Canon Bible) has to be implemented
	// in a subclass before these can ever be bonded with.
	const EAstralWildState AggressiveStates[] = {
		EAstralWildState::Territorial,
		EAstralWildState::Enraged,
	};

	for (EAstralWildState StartState : AggressiveStates)
	{
		AWildAstralEncounter* Encounter = NewObject<AWildAstralEncounter>();
		Encounter->SetWildState(StartState);

		Encounter->TryBecomeReceptive();

		TestEqual(*FString::Printf(TEXT("An aggressive Astral starting at %d does not move on its own"), (int32)StartState),
			(uint8)Encounter->GetWildState(), (uint8)StartState);
		TestFalse(*FString::Printf(TEXT("An aggressive Astral starting at %d is not receptive after the no-op"), (int32)StartState),
			Encounter->IsReceptive());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralWildEncounter_SetWildStateIsDirect, "AstralWilds.WildEncounter.SetWildState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralWildEncounter_SetWildStateIsDirect::RunTest(const FString& Parameters)
{
	// SetWildState is the escape hatch a subclass's real per-species behavior
	// (combat exhaustion, feeding, healing, a completed task, ...) is expected
	// to call once its own condition is actually satisfied - it must apply
	// immediately and unconditionally, including for the aggressive states
	// TryBecomeReceptive() itself refuses to move.
	AWildAstralEncounter* Encounter = NewObject<AWildAstralEncounter>();
	Encounter->SetWildState(EAstralWildState::Enraged);
	TestEqual(TEXT("SetWildState applies immediately"), (uint8)Encounter->GetWildState(), (uint8)EAstralWildState::Enraged);

	Encounter->SetWildState(EAstralWildState::Receptive);
	TestTrue(TEXT("SetWildState can move an Enraged Astral straight to Receptive"), Encounter->IsReceptive());

	return true;
}

#endif // WITH_AUTOMATION_TESTS
