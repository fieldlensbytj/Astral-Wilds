// Astral Wilds - see AstralWildSpawner.h.
#include "AstralWildSpawner.h"
#include "AstralCharacter.h"
#include "AstralSpeciesData.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Components/CapsuleComponent.h"

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
	bool bFoundNavPoint = false;
	if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
	{
		FNavLocation RandomLocation;
		if (NavSys->GetRandomReachablePointInRadius(GetActorLocation(), SpawnRadius, RandomLocation))
		{
			SpawnLocation = RandomLocation.Location;
			bFoundNavPoint = true;
		}
	}
	if (!bFoundNavPoint)
	{
		// No navmesh (or nothing reachable): pick a random point in the radius
		// and drop it onto whatever ground is there, rather than stacking every
		// Astral on the spawner itself.
		const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
		const FVector Probe = GetActorLocation() + FVector(Offset.X, Offset.Y, 0.f);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AstralWildSpawnerGround), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Probe + FVector(0.f, 0.f, 2000.f), Probe - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			const AAstralCharacter* DefaultAstral = AstralCharacterClass->GetDefaultObject<AAstralCharacter>();
			SpawnLocation = Hit.Location + FVector(0.f, 0.f, DefaultAstral->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 2.f);
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
