// Astral Wilds - automation tests for AAstralCharacter's bridge to
// FAstralCombatant (ToCombatant/InitFromCombatant) and its receptiveness
// state machine (moved in from AWildAstralEncounter). Like
// AstralWildEncounterTests, none of the methods under test touch GetWorld(),
// so this is pure NewObject<>() state-machine testing, no level or
// Play-in-Editor session required.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralCharacter.h"
#include "AstralSpeciesData.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCharacter_ToCombatantRoundTrip, "AstralWilds.Character.ToCombatant.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCharacter_ToCombatantRoundTrip::RunTest(const FString& Parameters)
{
	UAstralSpeciesData* Species = NewObject<UAstralSpeciesData>();
	Species->SpeciesName = FText::FromString(TEXT("Testling"));
	Species->PrimaryType = EAstralEssence::Volt;
	Species->BaseStats.HP = 20;
	Species->BaseStats.Attack = 8;
	Species->BaseStats.Defense = 6;
	Species->BaseStats.Speed = 12;
	Species->GrowthRatePerLevel = 0.10f;

	AAstralCharacter* Source = NewObject<AAstralCharacter>();
	Source->SpeciesData = Species;
	Source->Level = 3;
	Source->RecomputeStatsForLevel();

	// Scalar at level 3 = 1 + (3-1)*0.10 = 1.20 -> HP round(20*1.2) = 24.
	const int32 ExpectedMaxHp = Source->CurrentStats.HP;
	TestEqual(TEXT("Level-3 Testling has 24 max HP"), ExpectedMaxHp, 24);

	// Simulate battle damage before snapshotting/capturing.
	Source->CurrentHP = 10;

	const FAstralCombatant Snapshot = Source->ToCombatant();
	TestEqual(TEXT("Snapshot carries the species reference"), Snapshot.SpeciesData.Get(), Species);
	TestEqual(TEXT("Snapshot carries Level"), Snapshot.Level, 3);
	TestEqual(TEXT("Snapshot carries partial Hp, not full-healed"), Snapshot.Hp, 10);
	TestEqual(TEXT("Snapshot MaxHp matches computed stats"), Snapshot.MaxHp, ExpectedMaxHp);
	TestFalse(TEXT("Snapshot is not defeated at 10 Hp"), Snapshot.bDefeated);

	AAstralCharacter* Restored = NewObject<AAstralCharacter>();
	Restored->InitFromCombatant(Snapshot);
	TestEqual(TEXT("Restored species matches"), Restored->SpeciesData.Get(), Species);
	TestEqual(TEXT("Restored level matches"), Restored->Level, 3);
	TestEqual(TEXT("Restored Hp is carried over, not full-healed"), Restored->CurrentHP, 10);
	TestEqual(TEXT("Restored max HP recomputes identically"), Restored->CurrentStats.HP, ExpectedMaxHp);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCharacter_NonAggressiveStatesBecomeReceptive, "AstralWilds.Character.TryBecomeReceptive.NonAggressive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCharacter_NonAggressiveStatesBecomeReceptive::RunTest(const FString& Parameters)
{
	const EAstralWildState NonAggressiveStates[] = {
		EAstralWildState::Calm,
		EAstralWildState::Curious,
		EAstralWildState::Wary,
		EAstralWildState::Frightened,
	};

	for (EAstralWildState StartState : NonAggressiveStates)
	{
		AAstralCharacter* Character = NewObject<AAstralCharacter>();
		Character->SetWildState(StartState);

		Character->TryBecomeReceptive();

		TestEqual(*FString::Printf(TEXT("Starting from %d, TryBecomeReceptive() reaches Receptive"), (int32)StartState),
			(uint8)Character->WildState, (uint8)EAstralWildState::Receptive);
		TestTrue(*FString::Printf(TEXT("Starting from %d, IsReceptive() is true after TryBecomeReceptive()"), (int32)StartState),
			Character->IsReceptive());
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCharacter_AggressiveStatesRefuseToTransition, "AstralWilds.Character.TryBecomeReceptive.Aggressive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCharacter_AggressiveStatesRefuseToTransition::RunTest(const FString& Parameters)
{
	const EAstralWildState AggressiveStates[] = {
		EAstralWildState::Territorial,
		EAstralWildState::Enraged,
	};

	for (EAstralWildState StartState : AggressiveStates)
	{
		AAstralCharacter* Character = NewObject<AAstralCharacter>();
		Character->SetWildState(StartState);

		Character->TryBecomeReceptive();

		TestEqual(*FString::Printf(TEXT("An aggressive Astral starting at %d does not move on its own"), (int32)StartState),
			(uint8)Character->WildState, (uint8)StartState);
		TestFalse(*FString::Printf(TEXT("An aggressive Astral starting at %d is not receptive after the no-op"), (int32)StartState),
			Character->IsReceptive());
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
