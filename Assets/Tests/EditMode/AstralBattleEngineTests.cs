using System.Collections.Generic;
using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralBattleEngineTests
    {
        private static List<AstralDemoMember> MakeParty(int count = 2, int hp = 30)
        {
            var party = new List<AstralDemoMember>(count);
            for (int i = 0; i < count; i++)
                party.Add(new AstralDemoMember { id = "member-" + i, displayName = "Member " + i, hp = hp, maxHp = hp });
            return party;
        }

        private static AstralBattleEngine MakeEngine(List<AstralDemoMember> party, int opponentDamage = 6)
        {
            return new AstralBattleEngine(
                party, new[] { 0, 1 },
                "wild-a", "Wild A", "wild-b", "Wild B",
                opponentDamage);
        }

        /// <summary>
        /// Opponents always start at a fixed 30 HP regardless of the party's HP,
        /// so defeating one from a fresh engine takes multiple rounds. Both slots
        /// attack the same opponent slot each round (guarding is irrelevant here
        /// since only opponent HP matters), cycling rounds until it faints.
        /// </summary>
        private static void DefeatOpponent(AstralBattleEngine engine, int opponentSlot)
        {
            for (int round = 0; round < 10 && !engine.GetOpponent(opponentSlot).defeated; round++)
            {
                engine.QueueAttack(0, opponentSlot, out _);
                engine.QueueAttack(1, opponentSlot, out _);
                engine.TryFinishRound();
            }
        }

        [Test]
        public void Constructor_CreatesTwoOpponentsAndActivatesBothSides()
        {
            var party = MakeParty();
            var engine = MakeEngine(party);

            Assert.That(engine.GetOpponent(0).displayName, Is.EqualTo("Wild A"));
            Assert.That(engine.GetOpponent(1).displayName, Is.EqualTo("Wild B"));
            Assert.That(engine.Battle.PlayerSlots[0].Astral, Is.Not.Null);
            Assert.That(engine.Battle.PlayerSlots[1].Astral, Is.Not.Null);
            Assert.That(engine.Battle.OpponentSlots[0].Astral, Is.Not.Null);
            Assert.That(engine.Battle.OpponentSlots[1].Astral, Is.Not.Null);
        }

        [Test]
        public void QueueAttack_AppliesBasicAttackDamageAndMarksActed()
        {
            var engine = MakeEngine(MakeParty());
            var outcome = engine.QueueAttack(0, 0, out int damage);

            Assert.That(outcome, Is.EqualTo(AstralBattleEngine.ActionOutcome.Applied));
            Assert.That(damage, Is.EqualTo(AstralCombatRules.BasicAttackDamage));
            Assert.That(engine.GetOpponent(0).hp, Is.EqualTo(30 - AstralCombatRules.BasicAttackDamage));
            Assert.That(engine.HasActed(0), Is.True);
            Assert.That(engine.HasActed(1), Is.False);
        }

        [Test]
        public void QueueAttack_SameSlotTwiceInARound_IsRejected()
        {
            var engine = MakeEngine(MakeParty());
            engine.QueueAttack(0, 0, out _);
            var second = engine.QueueAttack(0, 1, out int damage);

            Assert.That(second, Is.EqualTo(AstralBattleEngine.ActionOutcome.SlotAlreadyActed));
            Assert.That(damage, Is.Zero);
        }

        [Test]
        public void QueueAttack_AgainstDefeatedTarget_IsInvalid()
        {
            var engine = MakeEngine(MakeParty());
            // Two hits of 12 does not defeat 30 HP; drive it down with repeated rounds instead.
            for (int i = 0; i < 3; i++)
            {
                engine.QueueAttack(0, 0, out _);
                engine.QueueGuard(1);
                engine.TryFinishRound();
            }
            Assert.That(engine.GetOpponent(0).defeated, Is.True);

            var outcome = engine.QueueAttack(1, 0, out int damage);
            Assert.That(outcome, Is.EqualTo(AstralBattleEngine.ActionOutcome.Invalid));
            Assert.That(damage, Is.Zero);
        }

        [Test]
        public void RepeatedAttacks_DefeatOpponentAndClampHpAtZeroAndSyncBattleState()
        {
            var engine = MakeEngine(MakeParty());
            DefeatOpponent(engine, 0);

            Assert.That(engine.GetOpponent(0).hp, Is.Zero, "HP must clamp at zero, never go negative.");
            Assert.That(engine.GetOpponent(0).defeated, Is.True);
            Assert.That(engine.Battle.OpponentParty.Astrals[0].IsDefeated, Is.True);
        }

        [Test]
        public void AllOpponentsDefeated_TrueOnlyWhenBothAreDefeated()
        {
            var engine = MakeEngine(MakeParty());
            Assert.That(engine.AllOpponentsDefeated(), Is.False);

            DefeatOpponent(engine, 0);
            Assert.That(engine.AllOpponentsDefeated(), Is.False, "Only one opponent is down.");

            DefeatOpponent(engine, 1);
            Assert.That(engine.AllOpponentsDefeated(), Is.True);
        }

        [Test]
        public void QueueArcBurst_HitsBothLivingOpponentsForPerTargetDamage()
        {
            var engine = MakeEngine(MakeParty());
            var outcome = engine.QueueArcBurst(0, out int targetsHit);

            Assert.That(outcome, Is.EqualTo(AstralBattleEngine.ActionOutcome.Applied));
            Assert.That(targetsHit, Is.EqualTo(2));
            Assert.That(engine.GetOpponent(0).hp, Is.EqualTo(30 - AstralCombatRules.ArcBurstDamagePerTarget));
            Assert.That(engine.GetOpponent(1).hp, Is.EqualTo(30 - AstralCombatRules.ArcBurstDamagePerTarget));
        }

        [Test]
        public void QueueArcBurst_SkipsAlreadyDefeatedOpponent()
        {
            var engine = MakeEngine(MakeParty());
            DefeatOpponent(engine, 0);

            var outcome = engine.QueueArcBurst(0, out int targetsHit);

            Assert.That(outcome, Is.EqualTo(AstralBattleEngine.ActionOutcome.Applied));
            Assert.That(targetsHit, Is.EqualTo(1));
        }

        [Test]
        public void QueueGuard_MarksGuardedAndActed()
        {
            var engine = MakeEngine(MakeParty());
            var outcome = engine.QueueGuard(0);

            Assert.That(outcome, Is.EqualTo(AstralBattleEngine.ActionOutcome.Applied));
            Assert.That(engine.IsGuarded(0), Is.True);
            Assert.That(engine.HasActed(0), Is.True);
        }

        [Test]
        public void TryFinishRound_WaitsUntilBothLivingActiveSlotsHaveActed()
        {
            var engine = MakeEngine(MakeParty());
            engine.QueueAttack(0, 0, out _);
            var pending = engine.TryFinishRound();

            Assert.That(pending.Resolved, Is.False, "Slot 1 has not acted yet.");
        }

        [Test]
        public void TryFinishRound_AppliesUnguardedCounterattackDamage()
        {
            var party = MakeParty();
            var engine = MakeEngine(party, opponentDamage: 6);
            engine.QueueAttack(0, 0, out _);
            engine.QueueGuard(1);
            engine.TryFinishRound();

            // Slot 0 acted with Attack (not Guard) so its counterattack lands unguarded.
            Assert.That(party[0].hp, Is.EqualTo(30 - 6));
            // Slot 1 guarded, so its counterattack is reduced to ceil(6/3) = 2.
            Assert.That(party[1].hp, Is.EqualTo(30 - 2));
        }

        [Test]
        public void TryFinishRound_ResetsActedAndGuardedForNextRound()
        {
            var engine = MakeEngine(MakeParty());
            engine.QueueAttack(0, 0, out _);
            engine.QueueGuard(1);
            engine.TryFinishRound();

            Assert.That(engine.HasActed(0), Is.False);
            Assert.That(engine.HasActed(1), Is.False);
            Assert.That(engine.IsGuarded(0), Is.False);
            Assert.That(engine.IsGuarded(1), Is.False);
        }

        [Test]
        public void TryFinishRound_RetargetsCounterattackWhenPrimaryTargetAlreadyDefeated()
        {
            var party = MakeParty(2, hp: 10);
            var engine = MakeEngine(party, opponentDamage: 12);

            // Round 1: slot 0 attacks unguarded (takes the full 12 and faints);
            // slot 1 guards (survives at 10 - ceil(12/3) = 6 HP).
            engine.QueueAttack(0, 0, out _);
            engine.QueueGuard(1);
            var round1 = engine.TryFinishRound();
            Assert.That(round1.Resolved, Is.True);
            Assert.That(party[0].defeated, Is.True, "Slot 0 should have fainted from the unguarded counterattack.");
            Assert.That(party[1].hp, Is.EqualTo(10 - 4));

            // Round 2: slot 0 is defeated and cannot act; slot 1 attacks unguarded.
            engine.QueueAttack(1, 0, out _);
            var round2 = engine.TryFinishRound();

            Assert.That(round2.Resolved, Is.True);
            // Opponent 0's counterattack would normally target slot 0, but that
            // slot already fainted, so it must retarget to the still-living
            // slot 1 instead of being silently dropped.
            Assert.That(round2.Summary, Does.Contain("Member 1"));
            Assert.That(round2.Summary, Does.Contain("fainted"));
            Assert.That(party[1].defeated, Is.True);
        }

        [Test]
        public void TryFinishRound_ReportsPlayerDefeatWhenWholePartyFaints()
        {
            var party = MakeParty(2, hp: 1);
            var engine = MakeEngine(party, opponentDamage: 6);
            engine.QueueAttack(0, 0, out _);
            engine.QueueAttack(1, 1, out _);
            var result = engine.TryFinishRound();

            Assert.That(result.Resolved, Is.True);
            Assert.That(result.PlayerDefeat, Is.True);
            Assert.That(party[0].defeated, Is.True);
            Assert.That(party[1].defeated, Is.True);
        }

        [Test]
        public void FindFirstEligibleParty_SkipsDefeatedMembers()
        {
            var party = MakeParty(3);
            party[0].defeated = true;

            Assert.That(AstralBattleEngine.FindFirstEligibleParty(party, 0), Is.EqualTo(1));
            Assert.That(AstralBattleEngine.FindFirstEligibleParty(party, 2), Is.EqualTo(2));
        }

        [Test]
        public void FindFirstEligibleParty_ReturnsNegativeOneWhenNoneEligible()
        {
            var party = MakeParty(2);
            party[0].defeated = true;
            party[1].defeated = true;

            Assert.That(AstralBattleEngine.FindFirstEligibleParty(party, 0), Is.EqualTo(-1));
        }

        [Test]
        public void AllDefeated_TrueOnlyWhenEveryMemberIsDefeated()
        {
            var party = MakeParty(2);
            Assert.That(AstralBattleEngine.AllDefeated(party), Is.False);

            party[0].defeated = true;
            Assert.That(AstralBattleEngine.AllDefeated(party), Is.False);

            party[1].defeated = true;
            Assert.That(AstralBattleEngine.AllDefeated(party), Is.True);
        }
    }
}
