#include "AstralCombatRules.h"

int32 UAstralCombatRules::ResolveIncomingDamage(int32 BaseDamage, bool bGuarded)
{
	if (BaseDamage <= 0)
	{
		return 0;
	}

	if (!bGuarded)
	{
		return BaseDamage;
	}

	// ceil(BaseDamage / 3), matching the validated Unity formula: (n + 2) / 3 with integer division.
	const int32 Reduced = (BaseDamage + 2) / 3;
	return FMath::Max(1, Reduced);
}

bool UAstralCombatRules::ApplyDamage(FAstralCombatant& Target, int32 Damage)
{
	if (Target.bDefeated || Damage <= 0)
	{
		return false;
	}

	Target.Hp = FMath::Max(0, Target.Hp - Damage);
	if (Target.Hp == 0)
	{
		Target.bDefeated = true;
		return true;
	}
	return false;
}
