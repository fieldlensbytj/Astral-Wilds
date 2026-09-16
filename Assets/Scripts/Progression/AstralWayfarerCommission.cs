using System;
using UnityEngine;

namespace AstralWilds
{
    /// <summary>A small post-expedition quest fueled entirely by gameplay-earned salvage.</summary>
    [Serializable]
    public sealed class AstralWayfarerCommission
    {
        public const int RequiredAlloySales = 2;
        public const long CompletionReward = 40;

        [SerializeField, Min(0)] private int alloySold;
        [SerializeField] private bool completed;

        public int AlloySold => alloySold;
        public int RemainingSales => Math.Max(0, RequiredAlloySales - alloySold);
        public bool IsComplete => completed;

        public bool TryRecordAlloySale(int quantity)
        {
            if (quantity <= 0 || alloySold > int.MaxValue - quantity)
                return false;

            alloySold += quantity;
            return true;
        }

        public bool TryComplete(bool unlocked, AstralWallet wallet)
        {
            if (!unlocked || completed || alloySold < RequiredAlloySales || wallet == null ||
                !wallet.TryEarn(CompletionReward, AstralCurrencySource.QuestReward))
                return false;

            completed = true;
            return true;
        }

        public bool TryRestore(int savedAlloySold, bool savedCompleted)
        {
            if (savedAlloySold < 0 || (savedCompleted && savedAlloySold < RequiredAlloySales))
                return false;

            alloySold = savedAlloySold;
            completed = savedCompleted;
            return true;
        }

        public void Reset()
        {
            alloySold = 0;
            completed = false;
        }
    }
}
