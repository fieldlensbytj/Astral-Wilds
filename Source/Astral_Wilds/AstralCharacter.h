// Astral Wilds - the runtime instance of a bonded/wild Astral creature (NOT
// the player - that's AAstralMageCharacter). Derives from ACharacter for
// built-in movement/collision since these will eventually roam and be
// AI-controlled. Holds a reference to its UAstralSpeciesData plus level/XP/HP
// and stats computed from that species data scaled by level.
//
// Bridges to the rest of the Astral Wilds systems via ToCombatant()/
// InitFromCombatant() (see AstralMageCharacter.h's Party/BattleEngine, which
// operate on FAstralCombatant) and owns the receptiveness state machine
// (EAstralWildState/IsReceptive/TryBecomeReceptive) moved in from
// AWildAstralEncounter, since that class is AActor-based (static placement)
// while wild Astrals now need to roam and be AI-controlled, same as this
// class. AWildAstralEncounter itself is left as-is (legacy/superseded, not
// deleted) so existing placements/tests keep working.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AstralSpeciesData.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralCharacter.generated.h"

class UStaticMeshComponent;
class USphereComponent;

UCLASS()
class AAstralCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AAstralCharacter();

	/** The species this instance belongs to. Assign a UAstralSpeciesData Blueprint child. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	TObjectPtr<UAstralSpeciesData> SpeciesData;

	/** Current level. Editable per-instance so individual Astrals can be previewed/placed at different levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral", meta = (ClampMin = "1"))
	int32 Level = 5;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astral")
	int32 CurrentXP = 0;

	/**
	 * Current HP. Recomputing stats (level or species change) resets this to
	 * full - there's no damage/persistence system yet, so "current" and "max"
	 * aren't distinguished until combat exists.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astral")
	int32 CurrentHP = 0;

	/** SpeciesData->BaseStats scaled by Level. Recomputed automatically when either changes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Astral")
	FAstralBaseStats CurrentStats;

	/** Recomputes CurrentStats/CurrentHP from SpeciesData and Level. Safe to call with a null SpeciesData (leaves stats zeroed). */
	UFUNCTION(BlueprintCallable, Category = "Astral")
	void RecomputeStatsForLevel();

	/** XP required to reach the next level, per SpeciesData's growth curve/formula. Returns 0 if SpeciesData is unset. */
	UFUNCTION(BlueprintPure, Category = "Astral")
	int32 GetXPToNextLevel() const;

	/** Builds a battle/roster-ready snapshot of this character's current state (species, level, XP, HP, stats, type). */
	UFUNCTION(BlueprintCallable, Category = "Astral")
	FAstralCombatant ToCombatant() const;

	/** Restores Species/Level/XP from a previously-saved FAstralCombatant (e.g. pulling a roster member back into the world). Carries over Hp rather than full-healing. */
	UFUNCTION(BlueprintCallable, Category = "Astral")
	void InitFromCombatant(const FAstralCombatant& Combatant);

	/** Current receptiveness state - see EAstralWildState. Moved in from AWildAstralEncounter (legacy). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Wild")
	EAstralWildState WildState = EAstralWildState::Calm;

	UFUNCTION(BlueprintPure, Category = "Astral|Wild")
	bool IsReceptive() const { return WildState == EAstralWildState::Receptive; }

	/**
	 * Moves this Astral toward Receptive, per its own species behavior. Base
	 * implementation is a simple direct transition (identical to
	 * AWildAstralEncounter's) - override in Blueprint/C++ for real per-species
	 * behavior. Aggressive states (Territorial/Enraged) refuse to transition on
	 * their own; a subclass must call SetWildState() directly once its own
	 * condition (combat/exhaustion, feeding, etc.) is satisfied.
	 */
	UFUNCTION(BlueprintCallable, Category = "Astral|Wild")
	virtual void TryBecomeReceptive();

	UFUNCTION(BlueprintCallable, Category = "Astral|Wild")
	void SetWildState(EAstralWildState NewState) { WildState = NewState; }

	/** Builds this species' Resonance Weave difficulty/feel. Falls back to a default-constructed temperament if SpeciesData is unset. */
	UFUNCTION(BlueprintPure, Category = "Astral|Bonding")
	FAstralWeaveTemperament GetWeaveTemperament() const { return SpeciesData ? SpeciesData->GetWeaveTemperament() : FAstralWeaveTemperament(); }

protected:

	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Procedural "life" for unrigged (static display mesh) Astrals until real
	 * rigs exist: breathing when idle, a gait bob scaled by speed, a lean into
	 * turns. Only touches the display mesh's relative transform.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	bool bProceduralMotion = true;

	/** Peak gait bob height at a full run, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals", meta = (Units = "cm"))
	float GaitBobHeight = 6.f;

	/** Distance covered per gait bob, in cm - shorter = quicker little steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals", meta = (Units = "cm"))
	float GaitStride = 90.f;

	/** Maximum roll when turning, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	float MaxTurnLean = 10.f;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** PLACEHOLDER visual: an engine basic-shape cylinder (no capsule ships with the engine) shown whenever SpeciesData has no DisplayMesh assigned yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	/** Overlap-only detection volume for AAstralMageCharacter::FindReceptiveWildAstral()'s sphere-overlap query. Originally mirrored the legacy AWildAstralEncounter's InteractSphere radius exactly; reduced independently 2026-10-05 (see AstralMageCharacter.h's InteractTraceDistance) after a playtest bot found combined interact reach of ~6.7m, large enough that the prompt showed from the level's PlayerStart. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> InteractSphere;

	/** Radius of InteractSphere. Reduced from 250 to 120 on 2026-10-05 as part of tightening the combined interact reach (see AstralMageCharacter.h). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Wild")
	float InteractRadius = 120.f;

	/** Applies SpeciesData's DisplayMesh/AnimClass to the inherited skeletal mesh if set; else its DisplayStaticMesh on PlaceholderMesh; else the placeholder cylinder. */
	void ApplySpeciesVisuals();

	/** The cylinder PlaceholderMesh shows when a species has no display model. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> PlaceholderShape;

	/** Rest transform of the static display mesh, set by ApplySpeciesVisuals; procedural motion offsets from it. */
	bool bHasStaticDisplay = false;
	FVector DisplayRestLocation = FVector::ZeroVector;
	FRotator DisplayRestRotation = FRotator::ZeroRotator;
	float DisplayRestScale = 1.f;

	float GaitPhase = 0.f;
	float BreathTime = 0.f;
	float SmoothedLean = 0.f;
	float SmoothedSpeedAlpha = 0.f;
	float LastYaw = 0.f;
};
