using NUnit.Framework;

namespace AstralWilds.Tests
{
    public sealed class AstralCombatRulesTests
    {
        [Test]
        public void ArcBurst_TradesFocusedDamageForTwoTargetPressure()
        {
            Assert.That(AstralCombatRules.ArcBurstDamagePerTarget, Is.LessThan(AstralCombatRules.BasicAttackDamage));
            Assert.That(AstralCombatRules.ArcBurstDamagePerTarget * 2, Is.GreaterThan(AstralCombatRules.BasicAttackDamage));
        }

        [TestCase(6, 6)]
        [TestCase(10, 10)]
        public void IncomingDamage_WhenUnguarded_UsesFullValue(int baseDamage, int expected)
        {
            Assert.That(AstralCombatRules.ResolveIncomingDamage(baseDamage, false), Is.EqualTo(expected));
        }

        [TestCase(1, 1)]
        [TestCase(6, 2)]
        [TestCase(10, 4)]
        public void IncomingDamage_WhenGuarded_ReducesToOneThirdRoundedUp(int baseDamage, int expected)
        {
            Assert.That(AstralCombatRules.ResolveIncomingDamage(baseDamage, true), Is.EqualTo(expected));
        }

        [TestCase(0)]
        [TestCase(-5)]
        public void IncomingDamage_NonPositiveValuesResolveToZero(int baseDamage)
        {
            Assert.That(AstralCombatRules.ResolveIncomingDamage(baseDamage, true), Is.Zero);
        }
    }
}
