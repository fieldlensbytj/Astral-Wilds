// Astral Wilds - the Resonance Weave bonding minigame (Canon Bible, Section
// VII). A short real-time interaction: the player TRACKS a moving Resonance
// Point inside a Sigil, HOLDS to channel the bond, and presses HARMONIZE
// when a resistance pulse reaches the center. No hidden dice roll decides
// the outcome behind the scenes - Stability only rises through good play,
// and reaching it is a genuine success.
//
// Later in the saga, bUseOldConcordance flips the fiction (and slightly
// changes the scoring curve) from "suppress the Astral's resistance" to
// "match the Astral's rhythm" - the same Track/Hold/Harmonize inputs, a
// different relationship underneath them.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AstralTypes.h"
#include "AstralResonanceWeaveComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralWeaveResult, EAstralWeaveResult, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralStabilityChanged, float, StabilityFraction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAstralPulse, float, PulseResponseWindowSeconds);

/**
 * Per-species tuning for a Resonance Weave attempt. Populate one of these
 * (from a data table or the Astral's own data asset) per wild encounter -
 * this is what makes a timid Astral feel different from an aggressive one
 * while the player's control scheme never changes.
 */
USTRUCT(BlueprintType)
struct FAstralWeaveTemperament
{
	GENERATED_BODY()

	/** How far/fast the Resonance Point drifts within the Sigil (0 = nearly still, 1 = erratic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	float Volatility = 0.35f;

	/** Seconds between resistance pulses. Shorter = more demanding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	float PulseInterval = 1.6f;

	/** How strong a missed pulse or lost alignment hurts Stability (0..1 scale, higher = harsher). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	float ResistanceStrength = 0.4f;

	/** Total Stability required to complete the bond. Rare/legendary Astrals should raise this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	float RequiredStability = 100.f;

	/** If true, a failed weave may cause the Astral to flee outright rather than allow a retry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	bool bMayFleeOnFailure = true;
};

/**
 * Attach to the Mage character (or a dedicated bonding actor). Drive it with
 * SetAlignmentInput each frame from mouse-delta/right-stick, SetChanneling
 * from the Channel action, and RespondToHarmonize from the Harmonize action.
 */
UCLASS(ClassGroup = (Astral), meta = (BlueprintSpawnableComponent))
class UAstralResonanceWeaveComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UAstralResonanceWeaveComponent();

	UPROPERTY(BlueprintAssignable, Category = "Astral|Bonding")
	FOnAstralWeaveResult OnWeaveResult;

	UPROPERTY(BlueprintAssignable, Category = "Astral|Bonding")
	FOnAstralStabilityChanged OnStabilityChanged;

	/** Fired the instant a resistance pulse reaches the center - the response window to Harmonize. */
	UPROPERTY(BlueprintAssignable, Category = "Astral|Bonding")
	FOnAstralPulse OnPulse;

	/**
	 * Stability every weave begins with, so the first missed pulse (12 * (1 +
	 * resistance) = 16.8 at the usual 0.4) or misaligned Harmonize (14) can't
	 * zero it out before the player has had a real chance - a 2026-10-05
	 * playtest bot fled at the very first pulse in 2 of 4 runs. Capped at half
	 * the temperament's RequiredStability so easy species never start more
	 * than halfway bonded.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding", meta = (ClampMin = "0"))
	float StartingStability = 20.f;

	/** Begins a weave attempt against a receptive Astral. Fails immediately (Result = MayRetry) if a weave is already in progress. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	void BeginWeave(const FAstralWeaveTemperament& Temperament, bool bUseOldConcordance);

	/** Cancels the current weave without resolving success or failure (e.g. the player walked away). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	void CancelWeave();

	/** Feed continuous alignment input each frame (mouse delta or right-stick axis), while a weave is active. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	void SetAlignmentInput(FVector2D Delta);

	/** Held while channeling the bond (Left Mouse / RT / R2). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	void SetChanneling(bool bNewChanneling);

	/** Called when the player presses Harmonize (Space / X / Square). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Bonding")
	void RespondToHarmonize();

	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	bool IsWeaveActive() const { return bIsActive; }

	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	float GetStabilityFraction() const { return CurrentTemperament.RequiredStability > 0.f ? FMath::Clamp(Stability / CurrentTemperament.RequiredStability, 0.f, 1.f) : 0.f; }

	/** Current Resonance Point position within the Sigil, normalized to a unit circle - for UI/VFX. */
	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	FVector2D GetResonancePoint() const { return ResonancePoint; }

	/** Current player alignment reticle position within the Sigil, normalized to a unit circle - for UI/VFX. */
	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	FVector2D GetAlignmentReticle() const { return PlayerAlignment; }

	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	bool IsChanneling() const { return bIsChanneling; }

	/** True between a resistance pulse firing and the Harmonize window closing (answered or missed). */
	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	bool IsAwaitingPulseResponse() const { return bAwaitingPulseResponse; }

	/** Fraction of the current Harmonize window still open, 1 -> 0. Zero when no pulse is live. */
	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	float GetPulseWindowFraction() const { return bAwaitingPulseResponse ? FMath::Clamp(PulseResponseTimeRemaining / PulseResponseWindow, 0.f, 1.f) : 0.f; }

	/** Radius, in unit-circle space, within which the reticle counts as aligned with the Resonance Point. */
	static float GetAlignmentToleranceRadius() { return AlignmentToleranceRadius; }

protected:

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:

	void ResolveWeave(EAstralWeaveResult Result);
	void UpdateResonancePointMovement(float DeltaTime);
	void UpdatePulseTimer(float DeltaTime);
	float ComputeAlignmentQuality() const;

	UPROPERTY()
	FAstralWeaveTemperament CurrentTemperament;

	bool bIsActive = false;
	bool bIsChanneling = false;
	bool bUseOldConcordance = false;

	/** True once the player has landed at least one successful Harmonize this weave - gates fleeing (see ResolveWeave's flee checks). A flighty Astral never flees on the very first pulse; it's a fixed grace period against the first-pulse difficulty cliff a playtest bot found 2026-10-05 (fled instantly at 1.6s in roughly half of its runs), not a change to the difficulty after that. */
	bool bHasSucceededAnyPulse = false;

	/** [-1, 1] on each axis; treated as a point within the unit circle. */
	FVector2D ResonancePoint = FVector2D::ZeroVector;
	FVector2D PlayerAlignment = FVector2D::ZeroVector;

	float Stability = 0.f;
	float TimeUntilNextPulse = 0.f;
	bool bAwaitingPulseResponse = false;
	float PulseResponseTimeRemaining = 0.f;

	/** Response window for Harmonize once a pulse fires, in seconds. */
	static constexpr float PulseResponseWindow = 0.45f;

	/** Radius, in unit-circle space, within which the player is considered "aligned" with the Resonance Point. */
	static constexpr float AlignmentToleranceRadius = 0.3f;

	FRandomStream MovementRandomStream;
	float MovementPhase = 0.f;
};
