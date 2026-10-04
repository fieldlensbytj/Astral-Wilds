// Astral Wilds - shared, stateless combat math for the Covenant of Two
// (Mage + 2 Astrals vs Mage + 2 Astrals). Ported from the validated Unity
// prototype (AstralCombatRules.cs / AstralBattleEngine.cs) so the same
// balancing carries over: a basic attack hits harder than Arc Burst per
// target, but Arc Burst covers two targets; guarding cuts incoming damage
// to roughly a third, rounded up, with a floor of 1 whenever any damage
// gets through at all.
#pragma once

#include "CoreMinimal.h"
#include "AstralTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstralCombatRules.generated.h"

UCLASS()
class UAstralCombatRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Damage dealt by a single-target basic attack. */
	static constexpr int32 BasicAttackDamage = 12;

	/** Damage dealt to EACH target by Arc Burst (hits both active opponents). */
	static constexpr int32 ArcBurstDamagePerTarget = 8;

	/**
	 * Resolves incoming damage against a target, applying the Guard reduction
	 * when the target guarded this round. Non-positive input damage resolves
	 * to zero; guarded damage is reduced to ceil(BaseDamage / 3), with a
	 * minimum of 1 whenever BaseDamage is positive.
	 */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	static int32 ResolveIncomingDamage(int32 BaseDamage, bool bGuarded);

	/** Applies damage to a combatant in place, clamping HP at zero and marking it defeated when it reaches zero. Returns true if this hit defeated the target. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	static bool ApplyDamage(UPARAM(ref) FAstralCombatant& Target, int32 Damage);
};
