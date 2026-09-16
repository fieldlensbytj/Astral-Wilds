using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralWalletTests
    {
        [Test]
        public void GameplayRewardsIncreaseBalance()
        {
            var wallet = new AstralWallet();
            Assert.IsTrue(wallet.TryEarn(50, AstralCurrencySource.BossVictory));
            Assert.IsTrue(wallet.TryEarn(25, AstralCurrencySource.ExplorationFind));
            Assert.AreEqual(75, wallet.Balance);
        }

        [Test]
        public void SpendRequiresPositiveAffordableAmount()
        {
            var wallet = new AstralWallet();
            wallet.TryEarn(40, AstralCurrencySource.QuestReward);
            Assert.IsFalse(wallet.TrySpend(0));
            Assert.IsFalse(wallet.TrySpend(41));
            Assert.IsTrue(wallet.TrySpend(15));
            Assert.AreEqual(25, wallet.Balance);
        }

        [Test]
        public void ItemSalesUseOnlyPositiveValueAndQuantity()
        {
            var wallet = new AstralWallet();
            Assert.IsFalse(wallet.TrySellItems(0, 3));
            Assert.IsFalse(wallet.TrySellItems(10, 0));
            Assert.IsTrue(wallet.TrySellItems(12, 3));
            Assert.AreEqual(36, wallet.Balance);
        }

        [Test]
        public void RestoreRejectsNegativeBalance()
        {
            var wallet = new AstralWallet();
            Assert.IsFalse(wallet.TryRestore(-1));
            Assert.AreEqual(0, wallet.Balance);
            Assert.IsTrue(wallet.TryRestore(125));
            Assert.AreEqual(125, wallet.Balance);
        }

        [Test]
        public void OverflowingRewardIsRejectedWithoutChangingBalance()
        {
            var wallet = new AstralWallet();
            Assert.IsTrue(wallet.TryRestore(long.MaxValue - 5));
            Assert.IsFalse(wallet.TryEarn(6, AstralCurrencySource.BossVictory));
            Assert.AreEqual(long.MaxValue - 5, wallet.Balance);
        }
    }
}
