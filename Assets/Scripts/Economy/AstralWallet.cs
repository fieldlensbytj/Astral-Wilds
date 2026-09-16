using System;
using UnityEngine;

namespace AstralWilds
{
    public enum AstralCurrencySource
    {
        BossVictory,
        EncounterClear,
        ExplorationFind,
        ItemSale,
        QuestReward
    }

    /// <summary>
    /// Earned-gameplay currency only. This domain object deliberately has no store,
    /// payment, entitlement, advertising, or premium-currency concepts.
    /// </summary>
    [Serializable]
    public sealed class AstralWallet
    {
        [SerializeField, Min(0f)] private long balance;

        public long Balance => balance;

        public bool TryEarn(long amount, AstralCurrencySource source)
        {
            if (amount <= 0 || balance > long.MaxValue - amount)
                return false;

            balance += amount;
            return true;
        }

        public bool TrySpend(long amount)
        {
            if (amount <= 0 || amount > balance)
                return false;

            balance -= amount;
            return true;
        }

        public bool TrySellItems(long unitValue, int quantity)
        {
            if (unitValue <= 0 || quantity <= 0 || unitValue > long.MaxValue / quantity)
                return false;

            return TryEarn(unitValue * quantity, AstralCurrencySource.ItemSale);
        }

        public bool TryRestore(long savedBalance)
        {
            if (savedBalance < 0)
                return false;

            balance = savedBalance;
            return true;
        }

        public void Reset()
        {
            balance = 0;
        }
    }
}
