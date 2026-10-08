// Astral Wilds - see AstralWildlifeController.h.
#include "AstralWildlifeController.h"
#include "AstralCharacter.h"
#include "Astral_Wilds.h"
#include "Components/StateTreeAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

namespace
{
	constexpr float DecisionInterval = 0.25f;
	constexpr float RepathInterval = 0.5f;
}

AAstralWildlifeController::AAstralWildlifeController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	// Ensure we start the StateTree ourselves once the pawn is fully possessed.
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);

	// Necessary for EnvQueries/navigation queries to work correctly.
	bAttachToPawn = true;

	PrimaryActorTick.bCanEverTick = true;

	// Arriving or switching mode lets the Astral brake (see AAstralCharacter's
	// movement setup) rather than zeroing its velocity in one frame.
	GetPathFollowingComponent()->SetStopMovementOnFinish(false);
}

void AAstralWildlifeController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	// Stagger decisions so a freshly spawned group doesn't move in lockstep.
	DecisionTimer = FMath::FRandRange(0.f, DecisionInterval);
	WanderPause = FMath::FRandRange(0.f, Tuning.WanderPauseMax);

	if (!bUseNativeBehavior)
	{
		StateTreeAI->StartLogic();
	}
}

EAstralWildlifeMode AAstralWildlifeController::ChooseMode(EAstralAIArchetype Archetype, EAstralWildState WildState, EAstralWildlifeMode Current,
	float DistToPlayer, float DistFromHome, float PlayerDistFromHome, const FAstralWildlifeTuning& T, bool bFleeStalled, bool bRecentlyCornered)
{
	if (WildState == EAstralWildState::Receptive)
	{
		return EAstralWildlifeMode::Idle;
	}

	switch (Archetype)
	{
	case EAstralAIArchetype::Skittish:
		if (Current == EAstralWildlifeMode::Flee)
		{
			// Keep fleeing until calm - unless it has stopped gaining distance
			// (cornered), in which case it settles where it is.
			return (DistToPlayer < T.CalmRange && !bFleeStalled) ? EAstralWildlifeMode::Flee : EAstralWildlifeMode::Wander;
		}
		// A recently cornered Astral only spooks again if the player comes much closer.
		if (DistToPlayer < (bRecentlyCornered ? T.AlertRange * 0.5f : T.AlertRange))
		{
			return EAstralWildlifeMode::Flee;
		}
		return EAstralWildlifeMode::Wander;

	case EAstralAIArchetype::Aggressive:
	{
		const bool bCanChase = DistFromHome < T.MaxChaseFromHome;
		if (bCanChase && (DistToPlayer < T.AlertRange || (Current == EAstralWildlifeMode::Chase && DistToPlayer < T.GiveUpRange)))
		{
			return EAstralWildlifeMode::Chase;
		}
		if ((Current == EAstralWildlifeMode::Chase || Current == EAstralWildlifeMode::ReturnHome) && DistFromHome > T.HomeRadius)
		{
			return EAstralWildlifeMode::ReturnHome;
		}
		return EAstralWildlifeMode::Wander;
	}

	case EAstralAIArchetype::Territorial:
		if (PlayerDistFromHome < T.TerritoryRadius && DistFromHome < T.MaxChaseFromHome)
		{
			return EAstralWildlifeMode::Chase;
		}
		// Patrols its territory (wanders around home); only heads straight
		// back after a chase. It used to stand still at home, which read as
		// a statue.
		if ((Current == EAstralWildlifeMode::Chase || Current == EAstralWildlifeMode::ReturnHome) && DistFromHome > T.HomeRadius)
		{
			return EAstralWildlifeMode::ReturnHome;
		}
		return EAstralWildlifeMode::Wander;

	case EAstralAIArchetype::Docile:
	default:
		return EAstralWildlifeMode::Wander;
	}
}

void AAstralWildlifeController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AAstralCharacter* Astral = Cast<AAstralCharacter>(GetPawn());
	if (!bUseNativeBehavior || !Astral)
	{
		return;
	}

	ModeTime += DeltaSeconds;
	RepathTimer += DeltaSeconds;
	DecisionTimer -= DeltaSeconds;
	if (DecisionTimer > 0.f)
	{
		return;
	}
	DecisionTimer = DecisionInterval;

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Here = Astral->GetActorLocation();
	const float DistToPlayer = Player ? FVector::Dist2D(Here, Player->GetActorLocation()) : TNumericLimits<float>::Max();
	const float PlayerDistFromHome = Player ? FVector::Dist2D(HomeLocation, Player->GetActorLocation()) : TNumericLimits<float>::Max();
	const EAstralAIArchetype Archetype = Astral->SpeciesData ? Astral->SpeciesData->AIArchetype : EAstralAIArchetype::Docile;

	bool bFleeStalled = false;
	if (Mode == EAstralWildlifeMode::Flee)
	{
		if (DistToPlayer > FleeBestDist + 50.f)
		{
			FleeBestDist = DistToPlayer;
			FleeLastProgressTime = ModeTime;
		}
		bFleeStalled = ModeTime > 4.f && ModeTime - FleeLastProgressTime > 2.f;
		if (bFleeStalled)
		{
			bRecentlyCornered = true;
		}
	}
	if (DistToPlayer > Tuning.CalmRange)
	{
		bRecentlyCornered = false;
	}

	const EAstralWildlifeMode NewMode = ChooseMode(Archetype, Astral->WildState, Mode, DistToPlayer, FVector::Dist2D(Here, HomeLocation), PlayerDistFromHome, Tuning,
		bFleeStalled, bRecentlyCornered);
	if (NewMode != Mode)
	{
		EnterMode(NewMode, Astral, DistToPlayer);
	}
	else if (Mode == EAstralWildlifeMode::Flee && FMath::FloorToInt(ModeTime / 2.f) != FMath::FloorToInt((ModeTime - DecisionInterval) / 2.f))
	{
		UE_LOG(LogAstral_Wilds, Display, TEXT("%s still fleeing after %.0fs: player %.0fcm, at %s, moving: %s"), *Astral->GetName(), ModeTime, DistToPlayer,
			*Here.ToCompactString(), GetMoveStatus() != EPathFollowingStatus::Idle ? TEXT("yes") : TEXT("no"));
	}
	UpdateMode(Astral, Player);
}

void AAstralWildlifeController::EnterMode(EAstralWildlifeMode NewMode, AAstralCharacter* Astral, float DistToPlayer)
{
	UE_LOG(LogAstral_Wilds, Display, TEXT("%s (%s): %s -> %s (player %.0fcm)"), *Astral->GetName(),
		Astral->SpeciesData ? *Astral->SpeciesData->SpeciesName.ToString() : TEXT("?"),
		*UEnum::GetValueAsString(Mode), *UEnum::GetValueAsString(NewMode), DistToPlayer);

	// A Skittish Astral that has calmed down settles where it ended up rather
	// than wandering straight back to the player it just fled from.
	if (Mode == EAstralWildlifeMode::Flee)
	{
		HomeLocation = Astral->GetActorLocation();
	}

	// Reflect reactions in WildState, but only ever between Calm and the
	// reaction state - never stomp a state set by something else (e.g. Receptive).
	if (Mode == EAstralWildlifeMode::Flee && Astral->WildState == EAstralWildState::Frightened)
	{
		Astral->SetWildState(EAstralWildState::Calm);
	}
	if (Mode == EAstralWildlifeMode::Chase && Astral->WildState == EAstralWildState::Territorial)
	{
		Astral->SetWildState(EAstralWildState::Calm);
	}
	if (Astral->WildState == EAstralWildState::Calm)
	{
		if (NewMode == EAstralWildlifeMode::Flee)
		{
			Astral->SetWildState(EAstralWildState::Frightened);
		}
		else if (NewMode == EAstralWildlifeMode::Chase)
		{
			Astral->SetWildState(EAstralWildState::Territorial);
		}
	}

	Mode = NewMode;
	ModeTime = 0.f;
	FleeBestDist = DistToPlayer;
	FleeLastProgressTime = 0.f;
	RepathTimer = RepathInterval; // act on the new mode immediately
	StopMovement();

	float Speed = Tuning.WanderSpeed;
	switch (NewMode)
	{
	case EAstralWildlifeMode::Flee:			Speed = Tuning.FleeSpeed; break;
	case EAstralWildlifeMode::Chase:		Speed = Tuning.ChaseSpeed; break;
	case EAstralWildlifeMode::ReturnHome:	Speed = Tuning.ChaseSpeed; break;
	default: break;
	}
	if (UCharacterMovementComponent* Movement = Astral->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Speed;
	}
}

bool AAstralWildlifeController::PickPoint(const FVector& Origin, float Radius, const APawn* Player, bool bFarthestFromPlayer, FVector& OutPoint) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	bool bFound = false;
	float BestScore = -1.f;
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FNavLocation Candidate;
		if (!NavSys->GetRandomReachablePointInRadius(Origin, Radius, Candidate))
		{
			continue;
		}
		const float PlayerDist = Player ? FVector::Dist2D(Candidate.Location, Player->GetActorLocation()) : TNumericLimits<float>::Max();
		if (bFarthestFromPlayer)
		{
			if (PlayerDist > BestScore)
			{
				BestScore = PlayerDist;
				OutPoint = Candidate.Location;
				bFound = true;
			}
		}
		else if (PlayerDist >= Tuning.AlertRange)
		{
			// Wandering: any point outside the player's alert range will do.
			OutPoint = Candidate.Location;
			return true;
		}
	}
	return bFound;
}

void AAstralWildlifeController::UpdateMode(AAstralCharacter* Astral, const APawn* Player)
{
	const bool bMoving = GetMoveStatus() != EPathFollowingStatus::Idle;

	switch (Mode)
	{
	case EAstralWildlifeMode::Wander:
		if (bMoving)
		{
			ModeTime = 0.f; // the pause counts from arrival, not departure
		}
		else if (ModeTime >= WanderPause)
		{
			FVector Target;
			if (PickPoint(HomeLocation, Tuning.RoamRadius, Player, /*bFarthestFromPlayer*/ false, Target))
			{
				MoveToLocation(Target, 15.f);   // small radius: the path's braking zone, not the radius, ends the walk
			}
			ModeTime = 0.f;
			WanderPause = FMath::FRandRange(Tuning.WanderPauseMin, Tuning.WanderPauseMax);
		}
		break;

	case EAstralWildlifeMode::Flee:
		if (Player && (!bMoving || RepathTimer >= RepathInterval))
		{
			RepathTimer = 0.f;
			// Best of several reachable escape points, so a wall behind it
			// doesn't leave it stuck trying to run straight through it.
			FVector Target;
			if (PickPoint(Astral->GetActorLocation(), Tuning.FleeDistance, Player, /*bFarthestFromPlayer*/ true, Target))
			{
				MoveToLocation(Target);
			}
		}
		break;

	case EAstralWildlifeMode::Chase:
		if (Player && RepathTimer >= RepathInterval)
		{
			RepathTimer = 0.f;
			MoveToLocation(Player->GetActorLocation(), Tuning.ChaseAcceptanceRadius);
		}
		break;

	case EAstralWildlifeMode::ReturnHome:
		if (!bMoving)
		{
			MoveToLocation(HomeLocation, Tuning.HomeRadius * 0.5f);
		}
		break;

	case EAstralWildlifeMode::Idle:
	default:
		break;
	}
}
