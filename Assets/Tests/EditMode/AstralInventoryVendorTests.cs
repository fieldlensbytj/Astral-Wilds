using NUnit.Framework;
using UnityEngine;

namespace AstralWilds.Tests
{
    public sealed class AstralInventoryVendorTests
    {
        [Test]
        public void Inventory_AddRemoveAndRestore_AreValidated()
        {
            var inventory = new AstralInventory();

            Assert.That(inventory.TryAdd(AstralItemId.SalvagedAlloy, 2), Is.True);
            Assert.That(inventory.TryRemove(AstralItemId.SalvagedAlloy, 1), Is.True);
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(1));
            Assert.That(inventory.TryRestore(4, 3), Is.True);
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(4));
            Assert.That(inventory.GetCount(AstralItemId.FieldTonic), Is.EqualTo(3));
            Assert.That(inventory.TryRestore(-1, 0), Is.False);
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(4));
        }

        [Test]
        public void BuyFieldTonic_SucceedsAtomically()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();
            Assert.That(wallet.TryEarn(45, AstralCurrencySource.EncounterClear), Is.True);

            Assert.That(AstralVendorService.TryBuyFieldTonic(wallet, inventory), Is.True);
            Assert.That(wallet.Balance, Is.EqualTo(15));
            Assert.That(inventory.GetCount(AstralItemId.FieldTonic), Is.EqualTo(1));
        }

        [Test]
        public void BuyFieldTonic_WhenUnaffordable_DoesNotMutateEitherSide()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();
            wallet.TryEarn(29, AstralCurrencySource.ExplorationFind);

            Assert.That(AstralVendorService.TryBuyFieldTonic(wallet, inventory), Is.False);
            Assert.That(wallet.Balance, Is.EqualTo(29));
            Assert.That(inventory.GetCount(AstralItemId.FieldTonic), Is.Zero);
        }

        [Test]
        public void SellSalvagedAlloy_SucceedsAtomically()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();
            inventory.TryAdd(AstralItemId.SalvagedAlloy, 2);

            Assert.That(AstralVendorService.TrySellSalvagedAlloy(wallet, inventory), Is.True);
            Assert.That(wallet.Balance, Is.EqualTo(AstralVendorService.SalvagedAlloySaleValue));
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(1));
        }

        [Test]
        public void SellSalvagedAlloy_WhenMissingItem_DoesNotMutateEitherSide()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();

            Assert.That(AstralVendorService.TrySellSalvagedAlloy(wallet, inventory), Is.False);
            Assert.That(wallet.Balance, Is.Zero);
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.Zero);
        }

        [Test]
        public void SellSalvagedAlloy_WhenWalletWouldOverflow_DoesNotMutateEitherSide()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();
            wallet.TryRestore(long.MaxValue - AstralVendorService.SalvagedAlloySaleValue + 1);
            inventory.TryAdd(AstralItemId.SalvagedAlloy, 1);

            Assert.That(AstralVendorService.TrySellSalvagedAlloy(wallet, inventory), Is.False);
            Assert.That(wallet.Balance, Is.EqualTo(long.MaxValue - AstralVendorService.SalvagedAlloySaleValue + 1));
            Assert.That(inventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(1));
        }

        [Test]
        public void EconomySaveData_JsonRoundTrip_RestoresCurrencyAndInventory()
        {
            var wallet = new AstralWallet();
            var inventory = new AstralInventory();
            wallet.TryEarn(125, AstralCurrencySource.BossVictory);
            inventory.TryRestore(3, 2);

            string json = JsonUtility.ToJson(AstralEconomySaveData.Capture(wallet, inventory));
            AstralEconomySaveData restoredData = JsonUtility.FromJson<AstralEconomySaveData>(json);
            var restoredWallet = new AstralWallet();
            var restoredInventory = new AstralInventory();

            Assert.That(restoredData.TryRestore(restoredWallet, restoredInventory), Is.True);
            Assert.That(restoredWallet.Balance, Is.EqualTo(125));
            Assert.That(restoredInventory.GetCount(AstralItemId.SalvagedAlloy), Is.EqualTo(3));
            Assert.That(restoredInventory.GetCount(AstralItemId.FieldTonic), Is.EqualTo(2));
        }
    }
}
