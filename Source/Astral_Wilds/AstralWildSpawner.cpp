// Astral Wilds - see AstralWildSpawner.h.
#include "AstralWildSpawner.h"
#include "AstralCharacter.h"
#include "AstralSpeciesData.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

AAstralWildSpawner::AAstralWildSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	AstralCharacterClass = AAstralCharacter::StaticClass();
}

void AAstralWildSpawner::BeginPlay()
{
	Super::BeginPlay();

	for (int32 Index = 0; Index < SpawnCount; ++Index)
	{
		SpawnOneAstral();
	}
}

AAstralCharacter* AAstralWildSpawner::SpawnOneAstral() const
{
	if (PossibleSpecies.Num() == 0 || !IsValid(AstralCharacterClass))
	{
		return nullptr;
	}

	UAstralSpeciesData* ChosenSpecies = PossibleSpecies[FMath::RandRange(0, PossibleSpecies.Num() - 1)];
	if (!ChosenSpecies)
	{
		return nullptr;
	}

	FVector SpawnLocation = GetActorLocation();
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation RandomLocation;
		if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, RandomLocation))
		{
			SpawnLocation = RandomLocation.Location;
		}
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

	// Deferred spawn so SpeciesData/Level are set before BeginPlay runs -
	// AAstralCharacter::BeginPlay() computes stats/visuals from them immediately.
	AAstralCharacter* NewAstral = GetWorld()->SpawnActorDeferred<AAstralCharacter>(
		AstralCharacterClass, SpawnTransform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!NewAstral)
	{
		return nullptr;
	}

	NewAstral->SpeciesData = ChosenSpecies;
	NewAstral->Level = FMath::RandRange(FMath::Min(MinLevel, MaxLevel), FMath::Max(MinLevel, MaxLevel));

	NewAstral->FinishSpawning(SpawnTransform);

	return NewAstral;
}
