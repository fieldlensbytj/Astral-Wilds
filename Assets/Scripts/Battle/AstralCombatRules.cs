using System;

namespace AstralWilds
{
    /// <summary>Pure combat calculations shared by the demo controller and tests.</summary>
    public static class AstralCombatRules
    {
        public static int ResolveIncomingDamage(int baseDamage, bool guarded)
        {
            if (baseDamage <= 0)
                return 0;

            return guarded ? Math.Max(1, (baseDamage + 2) / 3) : baseDamage;
        }
    }
}
