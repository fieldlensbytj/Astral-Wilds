using System;
using System.Collections.Generic;

namespace AstralWilds
{
    /// <summary>
    /// Owns the demo's 2v2 battle-round mechanics: queuing player actions (attack,
    /// Arc Burst, Guard), applying damage, and resolving each opponent
    /// counterattack round. Extracted from AstralDemoLoopController so the
    /// mechanics are unit-testable without a live Unity scene. Flow-state
    /// transitions (Encounter -> Battle -> Recruitment/Defeat), messages, save
    /// data, and rewards remain the controller's responsibility; this class only
    /// answers "what happened" so the controller can narrate and transition.
    /// </summary>
    public sealed class AstralBattleEngine
    {
        public enum ActionOutcome { Invalid, SlotAlreadyActed, Applied }

        public readonly struct RoundOutcome
        {
            public RoundOutcome(bool resolved, bool playerDefeat, string summary)
            {
                Resolved = resolved;
                PlayerDefeat = playerDefeat;
                Summary = summary;
            }

            /// <summary>False when one active slot still needs to act this round.</summary>
            public bool Resolved { get; }
            public bool PlayerDefeat { get; }
            public string Summary { get; }
        }

        private readonly IReadOnlyList<AstralDemoMember> party;
        private readonly int[] activeParty;
        private readonly AstralDemoMember[] opponents = new AstralDemoMember[2];
        private readonly bool[] acted = new bool[2];
        private readonly bool[] guarded = new bool[2];
        private readonly int opponentDamage;
        public AstralBattleState Battle { get; }

        public AstralBattleEngine(
            IReadOnlyList<AstralDemoMember> party,
            int[] activeParty,
            string primaryId, string primaryName,
            string companionId, string companionName,
            int opponentDamage)
        {
            this.party = party ?? throw new ArgumentNullException(nameof(party));
            this.activeParty = activeParty ?? throw new ArgumentNullException(nameof(activeParty));
            if (activeParty.Length != 2)
                throw new ArgumentException("Exactly two active slots are required.", nameof(activeParty));
            this.opponentDamage = Math.Max(1, opponentDamage);

            opponents[0] = new AstralDemoMember { id = primaryId, displayName = primaryName };
            opponents[1] = new AstralDemoMember { id = companionId, displayName = companionName };

            var playerParty = new AstralParty(AstralParty.PlayerCapacity);
            foreach (var member in party)
            {
                var combatant = new AstralCombatant(member.id, member.displayName);
                if (member.defeated) combatant.MarkDefeated();
                playerParty.TryAdd(combatant);
            }
            var enemyParty = new AstralParty(AstralParty.OpponentCapacity);
            foreach (var enemy in opponents)
                enemyParty.TryAdd(new AstralCombatant(enemy.id, enemy.displayName));

            Battle = new AstralBattleState(playerParty, enemyParty);
            for (int slot = 0; slot < 2; slot++)
            {
                Battle.TryActivate(BattleSide.Player, activeParty[slot], slot);
                Battle.TryActivate(BattleSide.Opponent, slot, slot);
            }
        }

        public AstralDemoMember GetOpponent(int slot) => (slot >= 0 && slot < 2) ? opponents[slot] : null;
        public bool HasActed(int slot) => slot >= 0 && slot < 2 && acted[slot];
        public bool IsGuarded(int slot) => slot >= 0 && slot < 2 && guarded[slot];

        public AstralDemoMember GetActiveParty(int slot)
        {
            if (slot < 0 || slot >= activeParty.Length || activeParty[slot] < 0 || activeParty[slot] >= party.Count)
                return null;
            return party[activeParty[slot]];
        }

        public bool AllOpponentsDefeated() => opponents[0] == null || (opponents[0].defeated && opponents[1].defeated);

        public ActionOutcome QueueAttack(int actorSlot, int targetSlot, out int damageDealt)
        {
            damageDealt = 0;
            AstralDemoMember actor = GetActiveParty(actorSlot);
            AstralDemoMember target = GetOpponent(targetSlot);
            if (actor == null || actor.defeated || target == null || target.defeated)
                return ActionOutcome.Invalid;
            if (acted[actorSlot] || !Battle.TryQueueAction(BattleSide.Player, actorSlot,
                new QueuedAstralAction("attack", TargetScope.OneEnemy, targetSlot)))
                return ActionOutcome.SlotAlreadyActed;

            acted[actorSlot] = true;
            damageDealt = AstralCombatRules.BasicAttackDamage;
            ApplyDamageToOpponent(targetSlot, damageDealt);
            return ActionOutcome.Applied;
        }

        public ActionOutcome QueueArcBurst(int actorSlot, out int targetsHit)
        {
            targetsHit = 0;
            AstralDemoMember actor = GetActiveParty(actorSlot);
            if (actor == null || actor.defeated)
                return ActionOutcome.Invalid;
            if (acted[actorSlot] || !Battle.TryQueueAction(BattleSide.Player, actorSlot,
                new QueuedAstralAction("arc-burst", TargetScope.BothEnemies)))
                return ActionOutcome.SlotAlreadyActed;

            acted[actorSlot] = true;
            for (int slot = 0; slot < opponents.Length; slot++)
            {
                if (opponents[slot] == null || opponents[slot].defeated)
                    continue;
                ApplyDamageToOpponent(slot, AstralCombatRules.ArcBurstDamagePerTarget);
                targetsHit++;
            }
            return ActionOutcome.Applied;
        }

        public ActionOutcome QueueGuard(int actorSlot)
        {
            AstralDemoMember actor = GetActiveParty(actorSlot);
            if (actor == null || actor.defeated)
                return ActionOutcome.Invalid;
            if (acted[actorSlot] || !Battle.TryQueueAction(BattleSide.Player, actorSlot,
                new QueuedAstralAction("guard", TargetScope.Self, actorSlot)))
                return ActionOutcome.SlotAlreadyActed;

            acted[actorSlot] = true;
            guarded[actorSlot] = true;
            return ActionOutcome.Applied;
        }

        /// <summary>
        /// Marks the actor slot as acted without a battle action (used by a
        /// voluntary bench swap, which still consumes the slot's turn).
        /// </summary>
        public void MarkActed(int slot)
        {
            if (slot >= 0 && slot < 2)
                acted[slot] = true;
        }

        private void ApplyDamageToOpponent(int slot, int damage)
        {
            AstralDemoMember target = opponents[slot];
            if (target == null || target.defeated || damage <= 0)
                return;

            target.hp = Math.Max(0, target.hp - damage);
            if (target.hp == 0)
            {
                target.defeated = true;
                Battle.OpponentParty.Astrals[slot].MarkDefeated();
            }
        }

        /// <summary>
        /// Resolves the round once both active party slots have acted (or are
        /// unable to). Returns Resolved=false if a slot is still waiting.
        /// </summary>
        public RoundOutcome TryFinishRound()
        {
            for (int slot = 0; slot < 2; slot++)
            {
                AstralDemoMember member = GetActiveParty(slot);
                if (member != null && !member.defeated && !acted[slot])
                    return new RoundOutcome(false, false, null);
            }

            string summary = ResolveOpponentActions();
            acted[0] = acted[1] = false;
            Battle.ClearQueuedActions();

            bool playerDefeat = AllDefeated(party);
            return new RoundOutcome(true, playerDefeat, summary);
        }

        private string ResolveOpponentActions()
        {
            var results = new List<string>(2);
            for (int i = 0; i < opponents.Length; i++)
            {
                AstralDemoMember enemy = opponents[i];
                int targetSlot = i % 2;
                AstralDemoMember target = GetActiveParty(targetSlot);
                if (target == null || target.defeated)
                {
                    targetSlot = (i + 1) % 2;
                    target = GetActiveParty(targetSlot);
                }
                if (enemy == null || enemy.defeated || target == null || target.defeated)
                    continue;

                bool wasGuarded = guarded[targetSlot];
                int damage = AstralCombatRules.ResolveIncomingDamage(opponentDamage, wasGuarded);
                target.hp = Math.Max(0, target.hp - damage);
                if (target.hp == 0)
                {
                    target.defeated = true;
                    int partyIndex = IndexOfMember(target);
                    if (partyIndex >= 0)
                        Battle.PlayerParty.Astrals[partyIndex].MarkDefeated();
                }
                results.Add($"{target.displayName}{(wasGuarded ? " guarded and" : "")} took {damage}{(target.defeated ? " and fainted" : "")}");
            }
            guarded[0] = guarded[1] = false;
            return results.Count == 0 ? "no counterattacks landed." : string.Join("; ", results) + ".";
        }

        private int IndexOfMember(AstralDemoMember member)
        {
            for (int i = 0; i < party.Count; i++)
                if (ReferenceEquals(party[i], member))
                    return i;
            return -1;
        }

        public static int FindFirstEligibleParty(IReadOnlyList<AstralDemoMember> party, int start)
        {
            for (int i = Math.Max(0, start); i < party.Count; i++)
                if (!party[i].defeated)
                    return i;
            return -1;
        }

        public static bool AllDefeated(IReadOnlyList<AstralDemoMember> party)
        {
            for (int i = 0; i < party.Count; i++)
                if (!party[i].defeated)
                    return false;
            return true;
        }
    }
}
