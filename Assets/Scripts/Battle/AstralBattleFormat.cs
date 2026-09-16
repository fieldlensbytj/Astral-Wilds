using System;
using System.Collections.Generic;
using UnityEngine;

namespace AstralWilds
{
    public enum BattleSide
    {
        Player,
        Opponent
    }

    public enum TargetScope
    {
        OneEnemy,
        EitherEnemy,
        BothEnemies,
        OneAlly,
        EitherAlly,
        BothAllies,
        Self,
        EntireBattlefield
    }

    [Serializable]
    public sealed class AstralCombatant
    {
        public AstralCombatant(string id, string displayName = null)
        {
            if (string.IsNullOrWhiteSpace(id))
                throw new ArgumentException("An Astral requires a stable id.", nameof(id));

            Id = id;
            DisplayName = string.IsNullOrWhiteSpace(displayName) ? id : displayName;
        }

        public string Id { get; }
        public string DisplayName { get; }
        public bool IsDefeated { get; private set; }
        public IReadOnlyCollection<string> StatusEffects => statusEffects;

        private readonly HashSet<string> statusEffects = new HashSet<string>(StringComparer.Ordinal);

        public void AddStatus(string statusId)
        {
            if (!string.IsNullOrWhiteSpace(statusId))
                statusEffects.Add(statusId);
        }

        public void MarkDefeated() => IsDefeated = true;
    }

    public sealed class AstralReserveCollection
    {
        private readonly List<AstralCombatant> astrals = new List<AstralCombatant>();

        public IReadOnlyList<AstralCombatant> Astrals => astrals;

        internal bool TryAdd(AstralCombatant astral)
        {
            if (astral == null || Contains(astral.Id))
                return false;

            astrals.Add(astral);
            return true;
        }

        internal AstralCombatant TakeAt(int index)
        {
            if (index < 0 || index >= astrals.Count)
                return null;
            AstralCombatant astral = astrals[index];
            astrals.RemoveAt(index);
            return astral;
        }

        private bool Contains(string id)
        {
            for (int i = 0; i < astrals.Count; i++)
                if (astrals[i].Id == id)
                    return true;
            return false;
        }
    }

    public sealed class AstralParty
    {
        public const int PlayerCapacity = 6;
        public const int OpponentCapacity = 6;

        private readonly List<AstralCombatant> astrals = new List<AstralCombatant>();

        public AstralParty(int capacity)
        {
            if (capacity < 1)
                throw new ArgumentOutOfRangeException(nameof(capacity));
            Capacity = capacity;
        }

        public int Capacity { get; }
        public IReadOnlyList<AstralCombatant> Astrals => astrals;
        public bool IsFull => astrals.Count >= Capacity;

        public bool TryAdd(AstralCombatant astral)
        {
            if (astral == null || IsFull || Contains(astral.Id))
                return false;

            astrals.Add(astral);
            return true;
        }

        public AstralCaptureResult Capture(AstralCombatant astral, AstralReserveCollection reserve)
        {
            if (astral == null)
                return AstralCaptureResult.Invalid;
            if (Contains(astral.Id))
                return AstralCaptureResult.Duplicate;
            if (TryAdd(astral))
                return AstralCaptureResult.AddedToParty;
            if (reserve != null && reserve.TryAdd(astral))
                return AstralCaptureResult.AddedToReserve;
            return AstralCaptureResult.Rejected;
        }

        public bool HasUsableAstral
        {
            get
            {
                for (int i = 0; i < astrals.Count; i++)
                    if (!astrals[i].IsDefeated)
                        return true;
                return false;
            }
        }

        private bool Contains(string id)
        {
            for (int i = 0; i < astrals.Count; i++)
                if (astrals[i].Id == id)
                    return true;
            return false;
        }
    }

    public enum AstralCaptureResult
    {
        Invalid,
        Duplicate,
        AddedToParty,
        AddedToReserve,
        Rejected
    }

    [Serializable]
    public sealed class QueuedAstralAction
    {
        public QueuedAstralAction(string actionId, TargetScope targetScope, int targetSlot = -1)
        {
            ActionId = actionId;
            TargetScope = targetScope;
            TargetSlot = targetSlot;
        }

        public string ActionId { get; }
        public TargetScope TargetScope { get; }
        public int TargetSlot { get; }
    }

    [Serializable]
    public sealed class ActiveAstralSlot
    {
        internal ActiveAstralSlot(int index)
        {
            Index = index;
        }

        public int Index { get; }
        public AstralCombatant Astral { get; internal set; }
        public int TargetSlot { get; internal set; } = -1;
        public Vector3 Position { get; internal set; }
        public QueuedAstralAction QueuedAction { get; internal set; }
        public IReadOnlyCollection<string> StatusEffects => Astral?.StatusEffects;
    }

    public sealed class AstralBattleState
    {
        public const int ActiveSlotsPerSide = 2;
        public const int MaximumActiveAstrals = 4;

        public AstralBattleState(AstralParty playerParty, AstralParty opponentParty)
        {
            PlayerParty = playerParty ?? throw new ArgumentNullException(nameof(playerParty));
            OpponentParty = opponentParty ?? throw new ArgumentNullException(nameof(opponentParty));
            PlayerSlots = CreateSlots();
            OpponentSlots = CreateSlots();
        }

        public AstralParty PlayerParty { get; }
        public AstralParty OpponentParty { get; }
        public IReadOnlyList<ActiveAstralSlot> PlayerSlots { get; }
        public IReadOnlyList<ActiveAstralSlot> OpponentSlots { get; }
        public int ActiveAstralCount => CountActive(PlayerSlots) + CountActive(OpponentSlots);

        public bool TryActivate(BattleSide side, int partyIndex, int activeSlot)
        {
            if (!IsValidSlot(activeSlot))
                return false;
            IReadOnlyList<AstralCombatant> party = GetParty(side).Astrals;
            if (partyIndex < 0 || partyIndex >= party.Count)
                return false;
            AstralCombatant astral = party[partyIndex];
            if (astral.IsDefeated || IsAlreadyActive(side, astral))
                return false;

            ActiveAstralSlot slot = GetSlots(side)[activeSlot];
            if (slot.Astral != null)
                return false;
            slot.Astral = astral;
            return true;
        }

        public bool TrySwitch(BattleSide side, int activeSlot, int replacementPartyIndex)
        {
            if (!IsValidSlot(activeSlot))
                return false;
            ActiveAstralSlot selectedSlot = GetSlots(side)[activeSlot];
            if (selectedSlot.Astral == null)
                return false;

            IReadOnlyList<AstralCombatant> party = GetParty(side).Astrals;
            if (replacementPartyIndex < 0 || replacementPartyIndex >= party.Count)
                return false;
            AstralCombatant replacement = party[replacementPartyIndex];
            if (replacement.IsDefeated || IsAlreadyActive(side, replacement))
                return false;

            selectedSlot.Astral = replacement;
            selectedSlot.TargetSlot = -1;
            selectedSlot.QueuedAction = null;
            return true;
        }

        public bool TrySwitchFromReserve(BattleSide side, int activeSlot, AstralReserveCollection reserve, int reserveIndex)
        {
            if (!IsValidSlot(activeSlot) || reserve == null)
                return false;
            ActiveAstralSlot selectedSlot = GetSlots(side)[activeSlot];
            if (selectedSlot.Astral == null)
                return false;
            AstralCombatant replacement = reserve.Astrals.Count > reserveIndex && reserveIndex >= 0 ? reserve.Astrals[reserveIndex] : null;
            if (replacement == null || replacement.IsDefeated || IsAlreadyActive(side, replacement))
                return false;
            replacement = reserve.TakeAt(reserveIndex);
            selectedSlot.Astral = replacement;
            selectedSlot.TargetSlot = -1;
            selectedSlot.QueuedAction = null;
            return true;
        }

        public bool TryQueueAction(BattleSide side, int activeSlot, QueuedAstralAction action)
        {
            if (!IsValidSlot(activeSlot) || action == null)
                return false;
            ActiveAstralSlot slot = GetSlots(side)[activeSlot];
            if (slot.Astral == null || slot.Astral.IsDefeated)
                return false;
            if (slot.QueuedAction != null)
                return false;
            slot.QueuedAction = action;
            slot.TargetSlot = action.TargetSlot;
            return true;
        }

        public bool HasBattleEnded => !PlayerParty.HasUsableAstral || !OpponentParty.HasUsableAstral;

        public void ClearQueuedActions()
        {
            foreach (var slot in PlayerSlots) { slot.QueuedAction = null; slot.TargetSlot = -1; }
            foreach (var slot in OpponentSlots) { slot.QueuedAction = null; slot.TargetSlot = -1; }
        }

        private static IReadOnlyList<ActiveAstralSlot> CreateSlots()
        {
            return new[] { new ActiveAstralSlot(0), new ActiveAstralSlot(1) };
        }

        private AstralParty GetParty(BattleSide side) => side == BattleSide.Player ? PlayerParty : OpponentParty;
        private IReadOnlyList<ActiveAstralSlot> GetSlots(BattleSide side) => side == BattleSide.Player ? PlayerSlots : OpponentSlots;
        private static bool IsValidSlot(int slot) => slot >= 0 && slot < ActiveSlotsPerSide;

        private bool IsAlreadyActive(BattleSide side, AstralCombatant astral)
        {
            IReadOnlyList<ActiveAstralSlot> slots = GetSlots(side);
            for (int i = 0; i < slots.Count; i++)
                if (slots[i].Astral == astral)
                    return true;
            return false;
        }

        private static int CountActive(IReadOnlyList<ActiveAstralSlot> slots)
        {
            int count = 0;
            for (int i = 0; i < slots.Count; i++)
                if (slots[i].Astral != null)
                    count++;
            return count;
        }
    }

    public readonly struct AstralBattleUiSnapshot
    {
        public AstralBattleUiSnapshot(AstralBattleState state)
        {
            PlayerActive = state.PlayerSlots;
            OpponentActive = state.OpponentSlots;
            PlayerParty = state.PlayerParty.Astrals;
        }

        public IReadOnlyList<ActiveAstralSlot> PlayerActive { get; }
        public IReadOnlyList<ActiveAstralSlot> OpponentActive { get; }
        public IReadOnlyList<AstralCombatant> PlayerParty { get; }
        public int PlayerPartySlotCount => AstralParty.PlayerCapacity;
        public int BattlefieldPositionCount => AstralBattleState.MaximumActiveAstrals;
    }
}
