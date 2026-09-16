using System;
using UnityEngine;

namespace AstralWilds
{
    public enum AstralItemId
    {
        SalvagedAlloy,
        FieldTonic
    }

    /// <summary>Versioned-save payload for the earned-only economy.</summary>
    [Serializable]
    public sealed class AstralEconomySaveData
    {
        public long starshards;
        public int salvagedAlloy;
        public int fieldTonics;

        public bool IsValid => starshards >= 0 && salvagedAlloy >= 0 && fieldTonics >= 0;

        public static AstralEconomySaveData Capture(AstralWallet wallet, AstralInventory inventory)
        {
            if (wallet == null) throw new ArgumentNullException(nameof(wallet));
            if (inventory == null) throw new ArgumentNullException(nameof(inventory));
            return new AstralEconomySaveData
            {
                starshards = wallet.Balance,
                salvagedAlloy = inventory.GetCount(AstralItemId.SalvagedAlloy),
                fieldTonics = inventory.GetCount(AstralItemId.FieldTonic)
            };
        }

        public bool TryRestore(AstralWallet wallet, AstralInventory inventory)
        {
            if (!IsValid || wallet == null || inventory == null)
                return false;

            // Validation makes both assignments guaranteed and keeps restoration atomic.
            return wallet.TryRestore(starshards) && inventory.TryRestore(salvagedAlloy, fieldTonics);
        }
    }

    /// <summary>
    /// Small deterministic inventory used by the field economy. Item counts are
    /// intentionally plain values so transactions and save validation stay auditable.
    /// </summary>
    [Serializable]
    public sealed class AstralInventory
    {
        [SerializeField, Min(0)] private int salvagedAlloy;
        [SerializeField, Min(0)] private int fieldTonics;

        public int GetCount(AstralItemId item) => item switch
        {
            AstralItemId.SalvagedAlloy => salvagedAlloy,
            AstralItemId.FieldTonic => fieldTonics,
            _ => 0
        };

        public bool CanAdd(AstralItemId item, int quantity)
        {
            if ((item != AstralItemId.SalvagedAlloy && item != AstralItemId.FieldTonic) || quantity <= 0)
                return false;

            return GetCount(item) <= int.MaxValue - quantity;
        }

        public bool TryAdd(AstralItemId item, int quantity)
        {
            if (!CanAdd(item, quantity))
                return false;

            SetCount(item, GetCount(item) + quantity);
            return true;
        }

        public bool TryRemove(AstralItemId item, int quantity)
        {
            if (quantity <= 0 || GetCount(item) < quantity)
                return false;

            SetCount(item, GetCount(item) - quantity);
            return true;
        }

        public bool TryRestore(int savedAlloy, int savedTonics)
        {
            if (savedAlloy < 0 || savedTonics < 0)
                return false;

            salvagedAlloy = savedAlloy;
            fieldTonics = savedTonics;
            return true;
        }

        public void Reset()
        {
            salvagedAlloy = 0;
            fieldTonics = 0;
        }

        private void SetCount(AstralItemId item, int value)
        {
            switch (item)
            {
                case AstralItemId.SalvagedAlloy: salvagedAlloy = value; break;
                case AstralItemId.FieldTonic: fieldTonics = value; break;
                default: throw new ArgumentOutOfRangeException(nameof(item), item, null);
            }
        }
    }
}
