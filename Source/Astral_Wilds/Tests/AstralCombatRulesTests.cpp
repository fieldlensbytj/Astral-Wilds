// Astral Wilds - automation tests for the stateless Covenant-of-Two combat
// math in AstralCombatRules. These are pure logic tests: no world, no
// actors, no Play-in-Editor required - matching the Fresh-Start
// Development Roadmap's "battle math ... should be checkable on its own"
// discipline, the same discipline the Unity prototype's EditMode NUnit
// suite (Assets/Tests/EditMode/AstralBattleFormatTests.cs) already applied
// before this logic was ported to C++.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralCombatRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCombatRules_ResolveIncomingDamage_Unguarded, "AstralWilds.Combat.ResolveIncomingDamage.Unguarded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCombatRules_ResolveIncomingDamage_Unguarded::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Unguarded damage passes through unchanged"), UAstralCombatRules::ResolveIncomingDamage(12, false), 12);
	TestEqual(TEXT("Zero base damage resolves to zero"), UAstralCombatRules::ResolveIncomingDamage(0, false), 0);
	TestEqual(TEXT("Negative base damage resolves to zero"), UAstralCombatRules::ResolveIncomingDamage(-5, false), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCombatRules_ResolveIncomingDamage_Guarded, "AstralWilds.Combat.ResolveIncomingDamage.Guarded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCombatRules_ResolveIncomingDamage_Guarded::RunTest(const FString& Parameters)
{
	// ceil(BaseDamage / 3), cross-checked against real verified play sessions in Docs/AI/WorkQueue.md:
	// the 6-damage Ember Hollow profile guards 30 -> 28 (reduction of 2), the 10-damage Stormbreak
	// profile guards 30 -> 26 (reduction of 4).
	TestEqual(TEXT("Guarded 6 damage reduces to 2 (matches documented Ember Hollow 30->28 guard result)"), UAstralCombatRules::ResolveIncomingDamage(6, true), 2);
	TestEqual(TEXT("Guarded 10 damage reduces to 4 (matches documented Stormbreak 30->26 guard result)"), UAstralCombatRules::ResolveIncomingDamage(10, true), 4);
	TestEqual(TEXT("Guarded 12 damage reduces to 4"), UAstralCombatRules::ResolveIncomingDamage(12, true), 4);
	TestEqual(TEXT("Guarded 1 damage floors to 1, never zero"), UAstralCombatRules::ResolveIncomingDamage(1, true), 1);
	TestEqual(TEXT("Guarded non-positive damage still resolves to zero"), UAstralCombatRules::ResolveIncomingDamage(0, true), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralCombatRules_ApplyDamage, "AstralWilds.Combat.ApplyDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralCombatRules_ApplyDamage::RunTest(const FString& Parameters)
{
	FAstralCombatant Target;
	Target.Hp = 30;
	Target.MaxHp = 30;
	Target.bDefeated = false;

	const bool bDefeatedByPartialHit = UAstralCombatRules::ApplyDamage(Target, 12);
	TestFalse(TEXT("A partial hit does not defeat the target"), bDefeatedByPartialHit);
	TestEqual(TEXT("HP drops by the damage amount"), Target.Hp, 18);
	TestFalse(TEXT("Target is not marked defeated after a partial hit"), Target.bDefeated);

	const bool bDefeatedByLethalHit = UAstralCombatRules::ApplyDamage(Target, 18);
	TestTrue(TEXT("A hit that brings HP to exactly zero defeats the target"), bDefeatedByLethalHit);
	TestEqual(TEXT("HP clamps at zero, never negative"), Target.Hp, 0);
	TestTrue(TEXT("Target is marked defeated once HP reaches zero"), Target.bDefeated);

	const bool bHitAfterDefeat = UAstralCombatRules::ApplyDamage(Target, 5);
	TestFalse(TEXT("Damage against an already-defeated target is a no-op"), bHitAfterDefeat);
	TestEqual(TEXT("HP does not go negative from a hit after defeat"), Target.Hp, 0);

	FAstralCombatant Overkill;
	Overkill.Hp = 10;
	Overkill.MaxHp = 30;
	const bool bOverkillDefeats = UAstralCombatRules::ApplyDamage(Overkill, 999);
	TestTrue(TEXT("Overkill damage still defeats the target"), bOverkillDefeats);
	TestEqual(TEXT("HP clamps at zero even for overkill damage"), Overkill.Hp, 0);

	FAstralCombatant Untouched;
	Untouched.Hp = 30;
	const bool bZeroDamage = UAstralCombatRules::ApplyDamage(Untouched, 0);
	TestFalse(TEXT("Zero damage does not defeat the target"), bZeroDamage);
	TestEqual(TEXT("Zero damage leaves HP unchanged"), Untouched.Hp, 30);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
