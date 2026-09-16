using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralWayfarerCommissionTests
    {
        [Test]
        public void TwoSales_AwardQuestRewardOnlyAfterUnlock()
        {
            var commission = new AstralWayfarerCommission();
            var wallet = new AstralWallet();
            commission.TryRecordAlloySale(2);

            Assert.That(commission.TryComplete(false, wallet), Is.False);
            Assert.That(wallet.Balance, Is.Zero);
            Assert.That(commission.TryComplete(true, wallet), Is.True);
            Assert.That(wallet.Balance, Is.EqualTo(AstralWayfarerCommission.CompletionReward));
            Assert.That(commission.IsComplete, Is.True);
            Assert.That(commission.TryComplete(true, wallet), Is.False);
            Assert.That(wallet.Balance, Is.EqualTo(AstralWayfarerCommission.CompletionReward));
        }

        [Test]
        public void IncompleteSales_DoNotAwardReward()
        {
            var commission = new AstralWayfarerCommission();
            var wallet = new AstralWallet();
            commission.TryRecordAlloySale(1);

            Assert.That(commission.TryComplete(true, wallet), Is.False);
            Assert.That(commission.RemainingSales, Is.EqualTo(1));
            Assert.That(wallet.Balance, Is.Zero);
        }

        [Test]
        public void WalletOverflow_PreventsCompletionWithoutLosingProgress()
        {
            var commission = new AstralWayfarerCommission();
            var wallet = new AstralWallet();
            commission.TryRecordAlloySale(2);
            wallet.TryRestore(long.MaxValue - AstralWayfarerCommission.CompletionReward + 1);

            Assert.That(commission.TryComplete(true, wallet), Is.False);
            Assert.That(commission.IsComplete, Is.False);
            Assert.That(commission.AlloySold, Is.EqualTo(2));
        }

        [Test]
        public void Restore_RejectsImpossibleCompletedStateWithoutMutation()
        {
            var commission = new AstralWayfarerCommission();
            commission.TryRecordAlloySale(1);

            Assert.That(commission.TryRestore(1, true), Is.False);
            Assert.That(commission.AlloySold, Is.EqualTo(1));
            Assert.That(commission.IsComplete, Is.False);
            Assert.That(commission.TryRestore(3, true), Is.True);
            Assert.That(commission.IsComplete, Is.True);
        }
    }
}
