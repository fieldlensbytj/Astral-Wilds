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
};
