// Astral Wilds - AI controller for roaming wild Astrals. Mirrors
// Variant_Combat/AI/CombatAIController.h exactly (owns a UStateTreeAIComponent,
// defers StartLogic() until OnPossess, bAttachToPawn=true for EQS/queries to
// work correctly). Not abstract, unlike ACombatAIController - there's no
// per-archetype controller subclassing here, the one shared StateTree graph
// (see AstralWildlifeStateTreeUtility.h) branches on SpeciesData->AIArchetype
// itself via FStateTreeAstralArchetypeCondition.
//
// Until that StateTree asset is authored, bUseNativeBehavior runs a simple
// C++ equivalent instead (wander / flee / chase / guard per archetype - see
// ChooseMode). Turn it off once a StateTree is assigned to StateTreeAI.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AstralTypes.h"
#include "AstralSpeciesData.h"
#include "AstralWildlifeController.generated.h"

class UStateTreeAIComponent;
class AAstralCharacter;

UENUM(BlueprintType)
enum class EAstralWildlifeMode : uint8
{
	Idle,
	Wander,
	Flee,
	Chase,
	ReturnHome
};

/**
 * Flying species only (UAstralSpeciesData::bCanFly): where it is in its
 * soar / land / rest / take-off cycle. Layered under EAstralWildlifeMode,
 * which still says what it wants (wander, chase, flee, go home).
 */
UENUM(BlueprintType)
enum class EAstralFlightPhase : uint8
{
	Grounded,
	TakingOff,
	Airborne,
	Landing
};

/**
 * What a wandering Astral is doing between and during its walks (TJ,
 * 2026-10-09: "more fluid and real life like"; behaviour read as robotic).
 * The anim instance reads it: grazing holds the nose to the ground, alert
 * holds the head up on the Mage and stops fidgeting.
 */
UENUM(BlueprintType)
enum class EAstralWanderActivity : uint8
{
	Travel,
	LookAround,
	Graze,
	Alert
};

/** Distances (cm) and speeds (cm/s) for the native wildlife behavior. Defaults match the StateTree task defaults where one exists. */
USTRUCT(BlueprintType)
struct FAstralWildlifeTuning
{
	GENERATED_BODY()

	/** Player closer than this triggers flee (Skittish) or chase (Aggressive). */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float AlertRange = 600.f;

	/** A fleeing Skittish Astral calms down once the player is farther than this. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float CalmRange = 1000.f;

	/** A chasing Astral gives up once the player is farther than this. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float GiveUpRange = 1200.f;

	/** A chasing Astral gives up once it is this far from home. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float MaxChaseFromHome = 1500.f;

	/** Territorial: the player entering this radius around its home triggers a chase. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float TerritoryRadius = 500.f;

	/** Considered "at home" within this distance of its home location. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float HomeRadius = 200.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float RoamRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float FleeDistance = 500.f;

	/** When a flee ends, the Astral walks on this far in the direction it was fleeing (slowing to a walk) before pausing. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float FleeRunOut = 400.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm"))
	float ChaseAcceptanceRadius = 150.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "s"))
	float WanderPauseMin = 2.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "s"))
	float WanderPauseMax = 5.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm/s"))
	float WanderSpeed = 140.f;   // a calm walk for the Astrals' leg length (200 read as scurrying)

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm/s"))
	float FleeSpeed = 450.f;

	/** Kept below the Mage's 500 walk speed so the player can always get away. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife", meta = (Units = "cm/s"))
	float ChaseSpeed = 350.f;

	/**
	 * Wary (TJ, 2026-10-09: Ripplefin "weary but not as skiddish as
	 * Glacielle"): it lets the player much closer before it moves off, backs
	 * away at a lope rather than bolting, and settles sooner. Between this
	 * and AlertRange it just keeps wandering, picking points away from the
	 * player, and the gaze keeps an eye on the Mage.
	 */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Wary", meta = (Units = "cm"))
	float WaryAlertRange = 350.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Wary", meta = (Units = "cm"))
	float WaryCalmRange = 700.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Wary", meta = (Units = "cm"))
	float WaryFleeDistance = 350.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Wary", meta = (Units = "cm"))
	float WaryFleeRunOut = 250.f;

	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Wary", meta = (Units = "cm/s"))
	float WaryFleeSpeed = 300.f;

	/** Wandering pulls away to the Mage's speed gently: a walk builds up over ~0.4s rather than snapping to pace. Flee and chase use the character's full acceleration. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Natural", meta = (Units = "cm/s^2"))
	float WanderAcceleration = 350.f;

	/** Each wander leg picks its pace from WanderSpeed x this range, and now and then a brisker one, so walks aren't all the same speed. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Natural")
	FVector2D WanderSpeedScale = FVector2D(0.8f, 1.1f);

	/** Share of pauses spent grazing / sniffing the ground (with small steps forward) rather than looking round. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Natural", meta = (ClampMin = "0", ClampMax = "1"))
	float GrazeChance = 0.5f;

	/** Skittish and Wary Astrals freeze and stare when the Mage comes within this multiple of their alert range, before deciding to bolt. */
	UPROPERTY(EditAnywhere, Category = "Astral|Wildlife|Natural")
	float NoticeRangeScale = 1.5f;
};

UCLASS()
class AAstralWildlifeController : public AAIController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

public:

	AAstralWildlifeController();

	/** Run the C++ wander/flee/chase/guard behavior instead of the StateTree. Turn off once a StateTree asset is assigned to StateTreeAI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Wildlife")
	bool bUseNativeBehavior = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Wildlife")
	FAstralWildlifeTuning Tuning;

	UFUNCTION(BlueprintPure, Category = "Astral|Wildlife")
	EAstralWildlifeMode GetMode() const { return Mode; }

	/**
	 * Pure decision rule for the native behavior - which mode a wild Astral
	 * should be in given its archetype, wild state and distances. Receptive
	 * Astrals always hold still so bonding is never a chase.
	 */
	static EAstralWildlifeMode ChooseMode(EAstralAIArchetype Archetype, EAstralWildState WildState, EAstralWildlifeMode Current,
		float DistToPlayer, float DistFromHome, float PlayerDistFromHome, const FAstralWildlifeTuning& Tuning,
		bool bFleeStalled = false, bool bRecentlyCornered = false);

	/** What it is doing while wandering (or Alert while frozen before a flee). */
	UFUNCTION(BlueprintPure, Category = "Astral|Wildlife")
	EAstralWanderActivity GetActivity() const { return Activity; }

	/**
	 * Where a wandering Astral walks next: on from where it is facing, turned
	 * by Turn (degrees, -1..1 of the spread), bent back toward home the
	 * farther it is from it, Distance away. Animals amble in curving lines,
	 * not between random points in a circle.
	 */
	static FVector MeanderTarget(const FVector& Here, const FVector& Facing, const FVector& Home, float RoamRadius, float Turn, float Distance);

	UFUNCTION(BlueprintPure, Category = "Astral|Wildlife")
	EAstralFlightPhase GetFlightPhase() const { return FlightPhase; }

	/**
	 * Pure decision rule for a flyer's phase. Anything that needs it in the
	 * air (a chase, a flee, heading home) gets it airborne; a wandering flyer
	 * soars for its air time then lands to rest for its ground time; a
	 * Receptive one (Mode Idle) lands and stays down so it can be bonded.
	 * RestTime is the current phase's air or ground time; bTouchedDown means
	 * a landing has reached the ground.
	 */
	static EAstralFlightPhase ChooseFlightPhase(EAstralFlightPhase Current, EAstralWildlifeMode Mode, float PhaseTime, float RestTime,
		float HeightAboveGround, float CruiseHeight, bool bTouchedDown);

	/** Height a landing flyer should be at, Distance (cm, horizontal) from its landing spot: a ~24 deg glide slope, capped at CruiseHeight. */
	static float GlideSlopeHeight(float Distance, float CruiseHeight) { return FMath::Clamp(Distance * 0.45f, 0.f, CruiseHeight); }

	/** What a soaring flyer is watching on the ground (an Astral or the Mage), or null. The anim instance locks the head on it. */
	const AActor* GetFlightQuarry() const { return Quarry.Get(); }

	/** How interesting a candidate quarry is (0 = out of range): nearer is better, moving things much more so, the Mage a little. */
	static float QuarryScore(float Distance, float Range, bool bMoving, bool bIsPlayer);

	/** A point on the soaring circle round Home, ahead of Here in the direction of travel (OrbitSign +1 anticlockwise, -1 clockwise). */
	static FVector OrbitTarget(const FVector& Here, const FVector& Home, float Radius, float OrbitSign);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void OnPossess(APawn* InPawn) override;

private:

	void EnterMode(EAstralWildlifeMode NewMode, AAstralCharacter* Astral, float DistToPlayer);

	/** Of several reachable points around Origin, the one farthest from the player (or any, with no player); false if none found. */
	bool PickPoint(const FVector& Origin, float Radius, const APawn* Player, bool bFarthestFromPlayer, FVector& OutPoint) const;
	void UpdateMode(AAstralCharacter* Astral, const APawn* Player);

	EAstralWildlifeMode Mode = EAstralWildlifeMode::Idle;
	FVector HomeLocation = FVector::ZeroVector;
	float DecisionTimer = 0.f;
	float ModeTime = 0.f;
	float WanderPause = 0.f;
	float RepathTimer = 0.f;

	/** Flee progress tracking: a fleeing Astral that stops gaining distance is cornered and settles instead of jittering in place. */
	float FleeBestDist = 0.f;
	float FleeLastProgressTime = 0.f;
	/** Set when a flee stalls; the Astral then only re-spooks inside half the alert range, until the player is past CalmRange. */
	bool bRecentlyCornered = false;

	/** Wander activity, graze steps left in this pause, the freeze before a flee, and when it may next stop to stare. */
	EAstralWanderActivity Activity = EAstralWanderActivity::LookAround;
	int32 GrazeSteps = 0;
	float AlertFreeze = 0.f;
	float NoticeCooldown = 0.f;
	/** The character's own MaxAcceleration (flee, chase); wandering uses Tuning.WanderAcceleration. */
	float DefaultAcceleration = 900.f;
	/** Picks the next wander leg (meander, pace) or the next graze step. */
	void StartWanderLeg(AAstralCharacter* Astral, const APawn* Player);
	void SetWalkPace(AAstralCharacter* Astral, float Speed, float Accel);

	/** Flight (bCanFly species only). */
	void EnterFlightPhase(EAstralFlightPhase NewPhase, AAstralCharacter* Astral);
	/** Per-frame flight steering: climb out, soar/chase/flee at height, glide down and flare to land. */
	void SteerFlight(AAstralCharacter* Astral, const APawn* Player, float DeltaSeconds);
	/** Ground height under the Astral (trace down), and false if there is no ground within reach. */
	bool GroundBelow(const AAstralCharacter* Astral, float& OutGroundZ) const;
	EAstralFlightPhase FlightPhase = EAstralFlightPhase::Grounded;
	float FlightPhaseTime = 0.f;
	float FlightRestTime = -1.f;   // < 0: not started yet (set on the first tick, once SpeciesData is known)
	float OrbitSign = 1.f;
	FVector LandingSpot = FVector::ZeroVector;
	bool bTouchedDown = false;

	/** Hunting from the air: the quarry it watches, when to look for another, and the (eased) centre of its circle. */
	void UpdateQuarry(AAstralCharacter* Astral, const APawn* Player, float DeltaSeconds);
	TWeakObjectPtr<const AActor> Quarry;
	float QuarryTimer = 0.f;
	FVector SoarCentre = FVector::ZeroVector;
	bool bHasSoarCentre = false;
};
