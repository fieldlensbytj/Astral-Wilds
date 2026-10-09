// Astral Wilds - species definitions. A UAstralSpeciesData is the designer-
// editable "what is this kind of Astral" record (type, base stats, ability
// slots, how it grows, how hard it is to bond with, what it looks like) that
// AAstralCharacter instances reference at runtime. Everything here is
// Blueprint-editable so new species never require touching C++.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "AstralTypes.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralSpeciesData.generated.h"

class UCurveFloat;
class USkeletalMesh;
class UStaticMesh;
class UAnimInstance;
class UAnimSequence;

/**
 * Per-species AI personality classification - drives how a wild Astral of
 * this species behaves before/during an encounter. Placeholder set; distinct
 * from EAstralWildState (AstralWildEncounter.h), which is the moment-to-moment
 * behavioral state a specific wild Astral is currently in.
 */
UENUM(BlueprintType)
enum class EAstralAIArchetype : uint8
{
	Aggressive	UMETA(DisplayName = "Aggressive"),
	Skittish	UMETA(DisplayName = "Skittish"),
	Territorial	UMETA(DisplayName = "Territorial"),
	Docile		UMETA(DisplayName = "Docile"),
	/** Cautious, not panicky: keeps its distance, and only backs off at a lope when the player gets close (Ripplefin). */
	Wary		UMETA(DisplayName = "Wary")
};

/**
 * How a flying species (Stormrook) flies: soars in circles round its home,
 * lands to rest a while, and takes off when disturbed, to chase, or when
 * rested. See AAstralWildlifeController's flight phases and the art repo's
 * Docs/Design/TurnReference.md for the bird references.
 */
USTRUCT(BlueprintType)
struct FAstralFlightTuning
{
	GENERATED_BODY()

	/** Height above the ground it soars at. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm"))
	float CruiseHeight = 650.f;

	/** Radius of its soaring circle round home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm"))
	float SoarRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm/s"))
	float CruiseSpeed = 520.f;

	/** Kept below the Mage's 500 walk speed so the player can always get away. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm/s"))
	float ChaseSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm/s"))
	float FleeSpeed = 650.f;

	/** Sideways acceleration a flying turn may use (cm/s^2): speed^2 / this is its tightest circle. Birds bank into wide arcs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm/s^2"))
	float TurnAcceleration = 700.f;

	/** Most roll into a flying turn, in degrees (birds bank far harder than runners lean). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight")
	float MaxBank = 40.f;

	/** How long it soars before coming down to rest, in seconds (random in range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "s"))
	float AirTimeMin = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "s"))
	float AirTimeMax = 30.f;

	/** How long it rests on the ground before taking off again, in seconds (random in range). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "s"))
	float GroundTimeMin = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "s"))
	float GroundTimeMax = 14.f;

	/**
	 * Hunting from the air (TJ, 2026-10-09: "track the monsters and people on
	 * the ground when it is flying similar to a real bird"): while soaring it
	 * picks something on the ground within this range (an Astral or the
	 * Mage, moving ones first), keeps its head locked on it, and circles
	 * over it, like a hawk working a field.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm"))
	float QuarryRange = 3000.f;

	/** The circle drifts over its quarry, but its centre stays within this of home. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm"))
	float QuarryLeash = 1500.f;

	/** Radius of the circle over a quarry (tighter than SoarRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Flight", meta = (Units = "cm"))
	float QuarryCircleRadius = 650.f;
};

/**
 * A species definition (not a runtime instance - see AAstralCharacter for
 * that). Create one Blueprint child of this class per species; no C++
 * required. PLACEHOLDER: all default values below are unbalanced placeholders
 * pending real design - there is no "Astral Wilds Canon Bible" file in the
 * repo despite other systems' comments referencing one, so nothing here had
 * an existing spec to match.
 */
UCLASS(BlueprintType)
class UAstralSpeciesData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(FName(TEXT("AstralSpecies")), GetFName()); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Identity")
	FText SpeciesName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Identity")
	EAstralEssence PrimaryType = EAstralEssence::Ember;

	/** Enables SecondaryType below - up to two types per Astral, Pokemon-style. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Identity")
	bool bHasSecondaryType = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Identity", meta = (EditCondition = "bHasSecondaryType"))
	EAstralEssence SecondaryType = EAstralEssence::Ember;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Stats")
	FAstralBaseStats BaseStats;

	/** Placeholder ability system: real ability data model TBD, gameplay tags stand in for now. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Abilities")
	TArray<FGameplayTag> AbilitySlots;

	/**
	 * How hard this Astral is to bond with, standard 0 (hardest) - 255
	 * (easiest) capture-rate convention. Combat isn't an independent dice
	 * roll here though - the real mechanic is the skill-based Resonance Weave
	 * (see AstralResonanceWeaveComponent.h), so this feeds GetWeaveTemperament()
	 * below rather than being consumed directly.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Capture", meta = (ClampMin = "0", ClampMax = "255", UIMin = "0", UIMax = "255"))
	int32 CaptureRate = 45;

	/** How far/fast the Resonance Point drifts within the Sigil - species "feel", independent of overall difficulty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Capture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaveVolatility = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Capture", meta = (ClampMin = "0.1"))
	float WeavePulseInterval = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Capture", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WeaveResistanceStrength = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Capture")
	bool bMayFleeOnWeaveFailure = true;

	/** Builds a real FAstralWeaveTemperament for this species: feel fields pass through, RequiredStability is derived from CaptureRate. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Capture")
	FAstralWeaveTemperament GetWeaveTemperament() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|AI")
	EAstralAIArchetype AIArchetype = EAstralAIArchetype::Docile;

	/**
	 * Per-species turning feel (art repo Docs/Design/TurnReference.md): the
	 * sideways acceleration a running turn may use, cm/s^2 - speed^2 / this
	 * is its tightest arc. A heavy boar (Ironbur) swings wide, a fox
	 * (Cindrel) cuts sharp. See UAstralMovementComponent::MaxTurnAcceleration.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Movement", meta = (Units = "cm/s^2", ClampMin = "100"))
	float TurnAcceleration = 800.f;

	/** Most it banks into a running turn, in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Movement", meta = (ClampMin = "0", ClampMax = "45"))
	float MaxTurnLean = 14.f;

	/** How far (deg) a rigged Astral's head and neck turn toward where it is steering, ahead of the body. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Movement", meta = (ClampMin = "0", ClampMax = "60"))
	float HeadLeadMax = 35.f;

	/** Flies (soars, lands to rest, takes off when disturbed) - see Flight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Movement")
	bool bCanFly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Movement", meta = (EditCondition = "bCanFly"))
	FAstralFlightTuning Flight;

	/** Optional curve mapping Level (X) -> XP required for that level (Y). Null-safe - see ComputeXPRequiredForLevel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Growth")
	TSoftObjectPtr<UCurveFloat> LevelToXPCurve;

	/** TUNABLE placeholder: +this fraction of each base stat per level above 1. No IV/EV/nature system yet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Growth", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float GrowthRatePerLevel = 0.10f;

	/** Scales BaseStats by Level using the TUNABLE placeholder growth formula above. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Growth")
	FAstralBaseStats ComputeStatsForLevel(int32 Level) const;

	/** Uses LevelToXPCurve if set; otherwise falls back to a TUNABLE placeholder cubic curve (Level^3). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Growth")
	int32 ComputeXPRequiredForLevel(int32 Level) const;

	/** Null-safe: Meshy/Blender assets drop in here later. Unset means AAstralCharacter shows its placeholder primitive instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	TSoftObjectPtr<USkeletalMesh> DisplayMesh;

	/** Null-safe, only used when DisplayMesh is also set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	TSoftClassPtr<UAnimInstance> AnimClass;

	/**
	 * Static (unrigged) display model, e.g. a Meshy export, used when the
	 * skeletal DisplayMesh is unset. Auto-fitted: uniformly scaled to
	 * DisplayHeight and placed with its base on the ground. No animation -
	 * the Astral glides until a rigged DisplayMesh replaces it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	TSoftObjectPtr<UStaticMesh> DisplayStaticMesh;

	/** Height in cm that DisplayStaticMesh is scaled to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals", meta = (ClampMin = "10", Units = "cm"))
	float DisplayHeight = 120.f;

	/** Yaw in degrees applied to the display model (static or skeletal) so it faces the Astral's forward (+X) direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals")
	float DisplayYawOffset = 0.f;

	/**
	 * Locomotion clips for a rigged DisplayMesh, crossfaded by ground speed
	 * by UAstralLocomotionAnimInstance (no Anim Blueprint needed): Idle fades
	 * into Walk above IdleSpeedThreshold, Walk into Run between WalkAnimSpeed
	 * and RunAnimSpeed. Walk and Run share one gait phase that advances with
	 * distance travelled. Ignored if AnimClass is set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> WalkAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> RunAnim;

	/** Flying species: wings beating (taking off, climbing, slow), played while airborne. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> FlyAnim;

	/** Flying species: wings held spread (cruising, soaring, diving). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> GlideAnim;

	/** Flying species: braking (wings swept forward against the air, nose up, feet forward) while slowing in the air, e.g. coming in to land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation")
	TSoftObjectPtr<UAnimSequence> FlareAnim;

	/** Ground speed (cm/s) at which WalkAnim's feet don't slide at 1x. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation", meta = (Units = "cm/s"))
	float WalkAnimSpeed = 150.f;

	/** Ground speed (cm/s) at which RunAnim's feet don't slide at 1x. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation", meta = (Units = "cm/s"))
	float RunAnimSpeed = 380.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Visuals|Animation", meta = (Units = "cm/s"))
	float IdleSpeedThreshold = 25.f;

	/** Drops the rigged DisplayMesh and its locomotion clips so the species falls back to DisplayStaticMesh. Also a button in the editor's Details panel. */
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Astral|Visuals|Animation")
	void ClearRiggedDisplay()
	{
		Modify();
		DisplayMesh.Reset();
		AnimClass.Reset();
		IdleAnim.Reset();
		WalkAnim.Reset();
		RunAnim.Reset();
		FlyAnim.Reset();
		GlideAnim.Reset();
		FlareAnim.Reset();
	}
};
