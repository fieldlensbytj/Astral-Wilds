// Astral Wilds - the Covenant of Two battle engine: Mage + 2 Astrals vs
// Mage + 2 Astrals (Canon Bible, Section VIII). Owns queuing player actions
// (attack, Arc Burst, guard), applying damage, and resolving each opponent
// counterattack round. Ported faithfully from the Unity prototype's
// validated AstralBattleEngine.cs so both engines share identical
// mechanics/balancing. Operates on a party array owned elsewhere (passed in
// by pointer) so damage/defeat state written here is visible to whatever
// UI, save system, or Astraldex code also reads that same party list -
// mirroring the reference semantics the C# version relied on.
#pragma once

#include "CoreMinimal.h"
#include "AstralTypes.h"
#include "AstralBattleEngine.generated.h"

/** Outcome of resolving a full round once both active party slots have acted. */
USTRUCT(BlueprintType)
struct FAstralRoundOutcome
{
	GENERATED_BODY()

	/** False when one active slot still needs to act this round. */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	bool bResolved = false;

	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	bool bPlayerDefeat = false;

	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	FString Summary;
};

UCLASS(BlueprintType)
class UAstralBattleEngine : public UObject
{
	GENERATED_BODY()

public:

	/**
	 * Sets up a fresh 2v2 encounter. Party is NOT copied - the engine writes
	 * HP/defeated state directly into it, the same array the controller's
	 * save/UI code should keep reading from. ActiveParty gives the two party
	 * indices currently in the fight.
	 */
	void Initialize(TArray<FAstralCombatant>* InParty, const int32 InActiveParty[2],
		const FString& PrimaryId, const FString& PrimaryName,
		const FString& CompanionId, const FString& CompanionName,
		int32 InOpponentDamage);

	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	FAstralCombatant GetOpponent(int32 Slot) const;

	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	bool HasActed(int32 Slot) const;

	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	bool IsGuarded(int32 Slot) const;

	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	bool AllOpponentsDefeated() const;

	/** Queues a basic attack from an active party slot against an opponent slot. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	EAstralActionOutcome QueueAttack(int32 ActorSlot, int32 TargetSlot, int32& OutDamageDealt);

	/** Queues Arc Burst - hits every living active opponent for reduced per-target damage. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	EAstralActionOutcome QueueArcBurst(int32 ActorSlot, int32& OutTargetsHit);

	/** Queues Guard - reduces the acting slot's incoming counterattack damage this round. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	EAstralActionOutcome QueueGuard(int32 ActorSlot);

	/** Marks a slot as having acted without a battle action (a voluntary bench swap still consumes the turn). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	void MarkActed(int32 Slot);

	/** Resolves the round once both active party slots have acted (or are unable to). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	FAstralRoundOutcome TryFinishRound();

	/** Returns the party index of the first non-defeated member at or after Start, or -1 if none remain. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	static int32 FindFirstEligibleParty(const TArray<FAstralCombatant>& Party, int32 Start);

	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	static bool AllDefeated(const TArray<FAstralCombatant>& Party);

private:

	void ApplyDamageToOpponent(int32 Slot, int32 Damage);
	FString ResolveOpponentActions();
	FAstralCombatant* GetActiveParty(int32 Slot) const;

	/** Not UPROPERTY: owned by whoever calls Initialize (typically the Mage character alongside its own party list); must outlive this engine. */
	TArray<FAstralCombatant>* Party = nullptr;

	int32 ActiveParty[2] = { 0, 1 };
	FAstralCombatant Opponents[2];
	bool bActed[2] = { false, false };
	bool bGuarded[2] = { false, false };
	int32 OpponentDamage = 6;
};
