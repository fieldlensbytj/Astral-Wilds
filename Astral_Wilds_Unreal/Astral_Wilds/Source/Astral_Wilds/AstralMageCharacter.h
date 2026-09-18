// Astral Wilds - the player's Mage. Extends the project's default third
// person character (movement/camera/jump, already wired for keyboard+mouse
// and gamepad via Enhanced Input) with the four Mage-specific actions from
// the Canon Bible: Attack, Arc Burst, Guard, and the Resonance Weave bonding
// interaction (Interact begins a weave on a receptive Astral; while weaving,
// the look input temporarily drives the Weave's alignment tracking instead
// of the camera, and Channel/Harmonize drive Hold/Harmonize).
#pragma once

#include "CoreMinimal.h"
#include "Astral_WildsCharacter.h"
#include "AstralTypes.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralBattleEngine.h"
#include "AstralMageCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Broadcast whenever the Mage's guard state changes, so HUD/animation can react. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralGuardChanged, bool, bIsGuarding);

/** Broadcast when the Mage lands a combat action, carrying the outcome for HUD/VFX. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAstralActionResolved, EAstralActionOutcome, Outcome, int32, DamageDealt);

/** Broadcast once both active slots have acted and the round resolves. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralRoundResolved, FAstralRoundOutcome, Outcome);

/** Broadcast when a battle ends, either by victory or by the player's party being wiped. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralBattleEnded, bool, bPlayerVictory);

/**
 *  The player-controlled Mage. Combat here follows the Covenant of Two: this
 *  character represents the Mage's own half of a Mage + 2 Astrals battle
 *  cell; the two active Astral combatants are tracked separately (by a
 *  future battle-manager actor) and are not part of this class.
 */
UCLASS()
class AAstralMageCharacter : public AAstral_WildsCharacter
{
	GENERATED_BODY()

protected:

	/** Attack Input Action - basic single-target strike. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral")
	UInputAction* AttackAction;

	/** Arc Burst Input Action - lighter damage split across both active opponents. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral")
	UInputAction* ArcBurstAction;

	/** Guard Input Action - held to reduce the next incoming hit. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral")
	UInputAction* GuardAction;

	/** Interact Input Action - begins a Resonance Weave on a receptive wild Astral, or interacts with the world otherwise. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral")
	UInputAction* InteractAction;

	/** Input Mapping Context active only while a Resonance Weave is in progress. Map its WeaveAlignmentAction to the SAME physical mouse/right-stick axis as the default Look action - Enhanced Input's context priority makes it take over cleanly, so the reticle replaces the camera for the duration of the weave. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral|Bonding")
	UInputMappingContext* ResonanceWeaveMappingContext;

	/** Priority given to ResonanceWeaveMappingContext when pushed; must be higher than the default context's priority. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral|Bonding")
	int32 ResonanceWeaveMappingPriority = 10;

	/** Weave alignment (Track) - bind to mouse movement / right stick within ResonanceWeaveMappingContext. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral|Bonding")
	UInputAction* WeaveAlignmentAction;

	/** Channel (Hold) - Left Mouse Button / RT / R2, within ResonanceWeaveMappingContext. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral|Bonding")
	UInputAction* ChannelAction;

	/** Harmonize - Space / X / Square, within ResonanceWeaveMappingContext. */
	UPROPERTY(EditAnywhere, Category = "Input|Astral|Bonding")
	UInputAction* HarmonizeAction;

	/** Drives the Track/Hold/Harmonize bonding minigame. See AstralResonanceWeaveComponent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astral|Bonding")
	UAstralResonanceWeaveComponent* ResonanceWeave;

	/** True while the Mage is holding Guard this round. */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Combat")
	bool bIsGuarding = false;

	/**
	 * The player's full Astral roster (up to 6, Canon Bible economy/registry
	 * scale aside - battle only ever uses two at a time per the Covenant).
	 * BattleEngine writes HP/defeated state directly into this array.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Party")
	TArray<FAstralCombatant> Party;

	/** Party indices of the two Astrals currently active in battle. Not Blueprint-exposed (UHT disallows static arrays on Blueprint properties) - use GetActiveParty(Slot) from Blueprint instead. */
	UPROPERTY()
	int32 ActiveParty[2] = { 0, 1 };

	/** Which of the two active slots the player is currently issuing orders for. */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	int32 SelectedActiveSlot = 0;

	/** Which opponent slot a basic attack will target. */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	int32 SelectedTargetSlot = 0;

	/** Non-null only while a Covenant-of-Two battle is in progress. */
	UPROPERTY(BlueprintReadOnly, Category = "Astral|Battle")
	UAstralBattleEngine* BattleEngine = nullptr;

public:

	AAstralMageCharacter();

	UPROPERTY(BlueprintAssignable, Category = "Astral|Combat")
	FOnAstralGuardChanged OnGuardChanged;

	UPROPERTY(BlueprintAssignable, Category = "Astral|Combat")
	FOnAstralActionResolved OnActionResolved;

	UPROPERTY(BlueprintAssignable, Category = "Astral|Battle")
	FOnAstralRoundResolved OnRoundResolved;

	UPROPERTY(BlueprintAssignable, Category = "Astral|Battle")
	FOnAstralBattleEnded OnBattleEnded;

protected:

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void HandleAttack(const FInputActionValue& Value);
	void HandleArcBurst(const FInputActionValue& Value);
	void HandleGuardStarted(const FInputActionValue& Value);
	void HandleGuardCompleted(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);
	void HandleWeaveAlignment(const FInputActionValue& Value);
	void HandleChannelStarted(const FInputActionValue& Value);
	void HandleChannelCompleted(const FInputActionValue& Value);
	void HandleHarmonize(const FInputActionValue& Value);

	UFUNCTION()
	void OnResonanceWeaveResult(EAstralWeaveResult Result);

public:

	/** Handles a basic attack from either real input or UI/mobile/AI. Override or extend in Blueprint to hook up targeting and the active Astral combatants. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	virtual void DoAttack();

	/** Handles Arc Burst from either real input or UI/mobile/AI. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	virtual void DoArcBurst();

	/** Begins guarding; reduces the next resolved incoming hit via UAstralCombatRules::ResolveIncomingDamage. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	virtual void DoGuardStart();

	/** Ends guarding at round resolution. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Combat")
	virtual void DoGuardEnd();

	/**
	 * Begins a Resonance Weave attempt against a receptive Astral, or a normal
	 * world interact if none is targeted. Pushes ResonanceWeaveMappingContext
	 * so Track/Hold/Harmonize take over from the camera for the duration.
	 * TODO: replace the default FAstralWeaveTemperament with the actual
	 * targeted Astral's species data once the wild-encounter system exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	virtual void DoInteract();

	UFUNCTION(BlueprintPure, Category = "Astral|Combat")
	bool IsGuarding() const { return bIsGuarding; }

	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	bool IsWeavingResonance() const { return ResonanceWeave && ResonanceWeave->IsWeaveActive(); }

	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	bool IsInBattle() const { return BattleEngine != nullptr; }

	/** Starts a Covenant-of-Two encounter against two named opponents. Party must already have at least one non-defeated member. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	virtual void BeginBattle(const FString& PrimaryId, const FString& PrimaryName,
		const FString& CompanionId, const FString& CompanionName, int32 OpponentDamage);

	/** Ends the current battle (victory, defeat, or fled) and clears BattleEngine. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	virtual void EndBattle();

	/** Picks which active slot the player is issuing orders for. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	void SelectActiveSlot(int32 Slot);

	/** Picks which opponent slot a basic attack will target. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Battle")
	void SelectTargetSlot(int32 Slot);

	/** Returns the party index occupying the given active battle slot (0 or 1), or -1 if out of range. Blueprint-safe alternative to reading ActiveParty directly. */
	UFUNCTION(BlueprintPure, Category = "Astral|Battle")
	int32 GetActiveSlotPartyIndex(int32 Slot) const { return (Slot >= 0 && Slot < 2) ? ActiveParty[Slot] : -1; }

protected:

	/** Called automatically after every queued action; resolves the round once both active slots have acted. */
	void FinishRoundIfReady();
};
