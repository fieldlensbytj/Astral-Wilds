// Astral Wilds - automation tests for the Covenant-of-Two round engine
// (UAstralBattleEngine). Exercises the same 2v2 queue/resolve flow the
// Unity prototype's AstralBattleEngine.cs validated (attack, Arc Burst,
// Guard, opponent counterattack, round resolution), cross-checked against
// real verified Play Mode / Play-in-Editor sessions logged in
// Docs/AI/WorkQueue.md so a regression here is caught before it reaches a
// hand-played session - per the roadmap's "verify every change actually
// compiles and runs" discipline. UAstralBattleEngine is a plain UObject
// that owns no actor/world state, so these run as simple automation tests
// with no level or Play-in-Editor session required.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralBattleEngine.h"
#include "AstralCombatRules.h"

namespace AstralBattleEngineTests
{
	TArray<FAstralCombatant> MakeParty()
	{
		TArray<FAstralCombatant> Party;

		FAstralCombatant Primary;
		Primary.Id = TEXT("primary");
		Primary.DisplayName = TEXT("Primary");
		Primary.Hp = Primary.MaxHp = 30;
		Party.Add(Primary);

		FAstralCombatant Companion;
		Companion.Id = TEXT("companion");
		Companion.DisplayName = TEXT("Companion");
		Companion.Hp = Companion.MaxHp = 30;
		Party.Add(Companion);

		return Party;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_QueueAttack, "AstralWilds.Battle.QueueAttack", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_QueueAttack::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();
	const int32 ActiveParty[2] = { 0, 1 };

	UAstralBattleEngine* Engine = NewObject<UAstralBattleEngine>();
	Engine->Initialize(&Party, ActiveParty, TEXT("ember"), TEXT("Ember"), TEXT("frost"), TEXT("Frost"), 6);

	int32 DamageDealt = 0;
	const EAstralActionOutcome Outcome = Engine->QueueAttack(0, 0, DamageDealt);
	TestEqual(TEXT("A valid attack is applied"), (uint8)Outcome, (uint8)EAstralActionOutcome::Applied);
	TestEqual(TEXT("A basic attack deals the documented 12 damage"), DamageDealt, UAstralCombatRules::BasicAttackDamage);
	TestEqual(TEXT("Target HP drops by the attack damage"), Engine->GetOpponent(0).Hp, 30 - UAstralCombatRules::BasicAttackDamage);
	TestTrue(TEXT("Acting slot is marked as having acted"), Engine->HasActed(0));

	int32 SecondDamage = 0;
	const EAstralActionOutcome RepeatOutcome = Engine->QueueAttack(0, 1, SecondDamage);
	TestEqual(TEXT("A slot cannot act twice in the same round"), (uint8)RepeatOutcome, (uint8)EAstralActionOutcome::SlotAlreadyActed);
	TestEqual(TEXT("No damage is dealt on a rejected repeat action"), SecondDamage, 0);
	TestEqual(TEXT("The untouched opponent's HP is unaffected"), Engine->GetOpponent(1).Hp, 30);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_QueueAttack_DefeatsOpponent, "AstralWilds.Battle.QueueAttack.DefeatsOpponent", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_QueueAttack_DefeatsOpponent::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();
	const int32 ActiveParty[2] = { 0, 1 };

	UAstralBattleEngine* Engine = NewObject<UAstralBattleEngine>();
	Engine->Initialize(&Party, ActiveParty, TEXT("ember"), TEXT("Ember"), TEXT("frost"), TEXT("Frost"), 6);

	// Three basic attacks at 12 damage each brings a 30 HP opponent to 30-12-12-12, clamped to 0.
	int32 Damage = 0;
	Engine->QueueAttack(0, 0, Damage);
	Engine->MarkActed(1);
	Engine->TryFinishRound();

	Engine->QueueAttack(0, 0, Damage);
	Engine->MarkActed(1);
	Engine->TryFinishRound();

	Engine->QueueAttack(0, 0, Damage);

	TestTrue(TEXT("An opponent reduced to zero HP is defeated"), Engine->GetOpponent(0).bDefeated);
	TestEqual(TEXT("Defeated opponent HP clamps at zero"), Engine->GetOpponent(0).Hp, 0);

	int32 DamageAfterDefeat = 0;
	const EAstralActionOutcome OutcomeAfterDefeat = Engine->QueueAttack(1, 0, DamageAfterDefeat);
	TestEqual(TEXT("Attacking an already-defeated opponent is rejected"), (uint8)OutcomeAfterDefeat, (uint8)EAstralActionOutcome::Invalid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_ArcBurstHitsLivingOpponentsOnly, "AstralWilds.Battle.ArcBurst.HitsLivingOpponentsOnly", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_ArcBurstHitsLivingOpponentsOnly::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();
	const int32 ActiveParty[2] = { 0, 1 };

	UAstralBattleEngine* Engine = NewObject<UAstralBattleEngine>();
	Engine->Initialize(&Party, ActiveParty, TEXT("ember"), TEXT("Ember"), TEXT("frost"), TEXT("Frost"), 6);

	int32 FirstTargetsHit = 0;
	Engine->QueueArcBurst(0, FirstTargetsHit);
	TestEqual(TEXT("Arc Burst hits both living opponents"), FirstTargetsHit, 2);
	TestEqual(TEXT("Each opponent takes the documented 8 Arc Burst damage"), Engine->GetOpponent(0).Hp, 30 - UAstralCombatRules::ArcBurstDamagePerTarget);
	TestEqual(TEXT("Each opponent takes the documented 8 Arc Burst damage"), Engine->GetOpponent(1).Hp, 30 - UAstralCombatRules::ArcBurstDamagePerTarget);
	Engine->MarkActed(1);
	Engine->TryFinishRound();

	// Finish off opponent 0 with two more basic attacks across two more rounds so only opponent 1 remains.
	int32 Damage = 0;
	Engine->QueueAttack(0, 0, Damage);
	Engine->MarkActed(1);
	Engine->TryFinishRound();

	Engine->QueueAttack(0, 0, Damage);
	TestTrue(TEXT("Opponent 0 is defeated after enough basic attacks"), Engine->GetOpponent(0).bDefeated);
	Engine->MarkActed(1);
	Engine->TryFinishRound();

	int32 SecondTargetsHit = 0;
	const int32 OpponentOneHpBefore = Engine->GetOpponent(1).Hp;
	Engine->QueueArcBurst(0, SecondTargetsHit);
	TestEqual(TEXT("Arc Burst only counts living opponents once one is defeated"), SecondTargetsHit, 1);
	TestEqual(TEXT("The defeated opponent takes no further damage"), Engine->GetOpponent(0).Hp, 0);
	TestEqual(TEXT("The surviving opponent still takes Arc Burst damage"), Engine->GetOpponent(1).Hp, OpponentOneHpBefore - UAstralCombatRules::ArcBurstDamagePerTarget);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_GuardReducesCounterattack, "AstralWilds.Battle.Guard.ReducesCounterattack", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_GuardReducesCounterattack::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();
	const int32 ActiveParty[2] = { 0, 1 };

	UAstralBattleEngine* Engine = NewObject<UAstralBattleEngine>();
	// The 6-damage profile matches the documented Ember Hollow encounter in Docs/AI/WorkQueue.md
	// (unguarded 30->24, guarded 30->28).
	Engine->Initialize(&Party, ActiveParty, TEXT("ember"), TEXT("Ember"), TEXT("frost"), TEXT("Frost"), 6);

	const EAstralActionOutcome GuardOutcome = Engine->QueueGuard(0);
	TestEqual(TEXT("Guard is a valid action"), (uint8)GuardOutcome, (uint8)EAstralActionOutcome::Applied);
	TestTrue(TEXT("Guard marks the slot as guarded"), Engine->IsGuarded(0));
	TestTrue(TEXT("Guard consumes the slot's action for the round"), Engine->HasActed(0));

	Engine->MarkActed(1);
	const FAstralRoundOutcome RoundOutcome = Engine->TryFinishRound();
	TestTrue(TEXT("Round resolves once both slots have acted"), RoundOutcome.bResolved);

	TestEqual(TEXT("Guarded slot takes reduced (ceil(6/3)=2) counterattack damage, matching the documented 30->28 result"), Party[0].Hp, 28);
	TestEqual(TEXT("Unguarded slot takes the full 6 counterattack damage, matching the documented 30->24 result"), Party[1].Hp, 24);
	TestFalse(TEXT("Guard flag clears after the round resolves"), Engine->IsGuarded(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_RoundOnlyResolvesOnceBothSlotsAct, "AstralWilds.Battle.TryFinishRound.WaitsForBothSlots", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_RoundOnlyResolvesOnceBothSlotsAct::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();
	const int32 ActiveParty[2] = { 0, 1 };

	UAstralBattleEngine* Engine = NewObject<UAstralBattleEngine>();
	Engine->Initialize(&Party, ActiveParty, TEXT("ember"), TEXT("Ember"), TEXT("frost"), TEXT("Frost"), 6);

	int32 Damage = 0;
	Engine->QueueAttack(0, 0, Damage);

	const FAstralRoundOutcome EarlyOutcome = Engine->TryFinishRound();
	TestFalse(TEXT("A round does not resolve while one active slot still hasn't acted"), EarlyOutcome.bResolved);
	TestEqual(TEXT("The un-acted opponent takes no counterattack damage before the round resolves"), Party[1].Hp, 30);

	Engine->MarkActed(1);
	const FAstralRoundOutcome FinalOutcome = Engine->TryFinishRound();
	TestTrue(TEXT("The round resolves once both slots have acted"), FinalOutcome.bResolved);
	TestFalse(TEXT("Neither party slot is defeated by a single 6-damage counterattack"), FinalOutcome.bPlayerDefeat);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralBattleEngine_StaticPartyHelpers, "AstralWilds.Battle.StaticPartyHelpers", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FAstralBattleEngine_StaticPartyHelpers::RunTest(const FString& Parameters)
{
	TArray<FAstralCombatant> Party = AstralBattleEngineTests::MakeParty();

	TestEqual(TEXT("The first eligible party member from index 0 is index 0"), UAstralBattleEngine::FindFirstEligibleParty(Party, 0), 0);
	TestFalse(TEXT("A party with a living member is not all-defeated"), UAstralBattleEngine::AllDefeated(Party));

	Party[0].bDefeated = true;
	TestEqual(TEXT("Skips a defeated member to find the next eligible one"), UAstralBattleEngine::FindFirstEligibleParty(Party, 0), 1);

	Party[1].bDefeated = true;
	TestEqual(TEXT("Returns -1 when no eligible member remains"), UAstralBattleEngine::FindFirstEligibleParty(Party, 0), -1);
	TestTrue(TEXT("A party with every member defeated is all-defeated"), UAstralBattleEngine::AllDefeated(Party));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
