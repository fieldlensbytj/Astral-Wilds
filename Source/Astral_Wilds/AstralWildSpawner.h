// Astral Wilds - a basic spawner that populates the level with wild Astrals
// on BeginPlay. Deliberately simpler than Variant_Combat/AI/CombatEnemySpawner
// (no wave/depleted/activatable logic) - just "populate the level."
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstralWildSpawner.generated.h"

class AAstralCharacter;
class UAstralSpeciesData;

UCLASS()
class AAstralWildSpawner : public AActor
{
	GENERATED_BODY()

public:

	AAstralWildSpawner();

	/** Class to spawn. Defaults to the base AAstralCharacter - override only if a Blueprint child is needed later. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner")
	TSubclassOf<AAstralCharacter> AstralCharacterClass;

	/** Random pool to pick a species from for each spawned Astral. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner")
	TArray<TObjectPtr<UAstralSpeciesData>> PossibleSpecies;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner", meta = (ClampMin = "0"))
	int32 SpawnCount = 5;

	/** Radius around this spawner's location within which spawn points are chosen (nav-mesh reachable, falls back to this actor's own location if none found). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner", meta = (ClampMin = "0", Units = "cm"))
	float SpawnRadius = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner", meta = (ClampMin = "1"))
	int32 MinLevel = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Spawner", meta = (ClampMin = "1"))
	int32 MaxLevel = 8;

protected:

	virtual void BeginPlay() override;

	/** Picks a random species/level/nav-valid point and spawns one AAstralCharacter. Returns null if PossibleSpecies is empty or the class is invalid. */
	AAstralCharacter* SpawnOneAstral() const;
};
