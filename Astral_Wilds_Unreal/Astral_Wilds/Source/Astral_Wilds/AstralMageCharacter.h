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
#include "AstralMageCharacter.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Broadcast whenever the Mage's guard state changes, so HUD/animation can react. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralGuardChanged, bool, bIsGuarding);

/** Broadcast when the Mage lands a combat action, carrying the outcome for HUD/VFX. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAstralActionResolved, EAstralActionOutcome, Outcome, int32, DamageDealt);

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

public:

	AAstralMageCharacter();

	UPROPERTY(BlueprintAssignable, Category = "Astral|Combat")
	FOnAstralGuardChanged OnGuardChanged;

	UPROPERTY(BlueprintAssignable, Category = "Astral|Combat")
	FOnAstralActionResolved OnActionResolved;

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
};
