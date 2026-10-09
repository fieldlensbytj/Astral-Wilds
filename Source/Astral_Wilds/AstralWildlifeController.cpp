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
	constexpr float FlyerWalkSpeed = 70.f;   // cm/s: Stormrook's walk clip is authored at ~60
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

	// Flyers steer every frame while off the ground (input is consumed each
	// movement tick); the 0.25s decisions below only pick what to do.
	const bool bFlyer = Astral->SpeciesData && Astral->SpeciesData->bCanFly;
	if (bFlyer)
	{
		if (FlightRestTime < 0.f)
		{
			FlightRestTime = FMath::FRandRange(1.5f, 4.f);   // just spawned on the ground: take off soon
		}
		FlightPhaseTime += DeltaSeconds;
		if (FlightPhase != EAstralFlightPhase::Grounded)
		{
			SteerFlight(Astral, UGameplayStatics::GetPlayerPawn(this, 0), DeltaSeconds);
		}
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

	if (bFlyer)
	{
		float GroundZ = 0.f;
		const float Height = GroundBelow(Astral, GroundZ) ? Here.Z - Astral->GetSimpleCollisionHalfHeight() - GroundZ : TNumericLimits<float>::Max();
		const EAstralFlightPhase NewPhase = ChooseFlightPhase(FlightPhase, Mode, FlightPhaseTime, FlightRestTime, Height,
			Astral->SpeciesData->Flight.CruiseHeight, bTouchedDown);
		if (NewPhase != FlightPhase)
		{
			EnterFlightPhase(NewPhase, Astral);
		}
		if (FlightPhase != EAstralFlightPhase::Grounded)
		{
			return;   // steered per frame by SteerFlight, not along nav paths
		}
	}
	UpdateMode(Astral, Player);
}

EAstralFlightPhase AAstralWildlifeController::ChooseFlightPhase(EAstralFlightPhase Current, EAstralWildlifeMode Mode, float PhaseTime, float RestTime,
	float HeightAboveGround, float CruiseHeight, bool bTouchedDown)
{
	const bool bReceptive = Mode == EAstralWildlifeMode::Idle;
	const bool bNeedsAir = Mode == EAstralWildlifeMode::Chase || Mode == EAstralWildlifeMode::Flee || Mode == EAstralWildlifeMode::ReturnHome;
	switch (Current)
	{
	case EAstralFlightPhase::Grounded:
		if (bReceptive)
		{
			return Current;
		}
		return (bNeedsAir || PhaseTime >= RestTime) ? EAstralFlightPhase::TakingOff : Current;

	case EAstralFlightPhase::TakingOff:
		if (bReceptive)
		{
			return EAstralFlightPhase::Landing;
		}
		return (HeightAboveGround >= CruiseHeight * 0.6f || PhaseTime > 3.f) ? EAstralFlightPhase::Airborne : Current;

	case EAstralFlightPhase::Airborne:
		if (bReceptive || (Mode == EAstralWildlifeMode::Wander && PhaseTime >= RestTime))
		{
			return EAstralFlightPhase::Landing;
		}
		return Current;

	case EAstralFlightPhase::Landing:
	default:
		if (bTouchedDown)
		{
			return EAstralFlightPhase::Grounded;
		}
		// Disturbed on the way down: back up.
		return (Mode == EAstralWildlifeMode::Chase || Mode == EAstralWildlifeMode::Flee) ? EAstralFlightPhase::Airborne : Current;
	}
}

FVector AAstralWildlifeController::OrbitTarget(const FVector& Here, const FVector& Home, float Radius, float Sign)
{
	const FVector Off(Here.X - Home.X, Here.Y - Home.Y, 0.f);
	const float Angle = (Off.IsNearlyZero() ? 0.f : FMath::Atan2(Off.Y, Off.X)) + (Sign >= 0.f ? 0.6f : -0.6f);   // aim ~35 deg round the circle ahead
	return FVector(Home.X + FMath::Cos(Angle) * Radius, Home.Y + FMath::Sin(Angle) * Radius, Here.Z);
}

bool AAstralWildlifeController::GroundBelow(const AAstralCharacter* Astral, float& OutGroundZ) const
{
	FHitResult Hit;
	const FVector From = Astral->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AstralGroundBelow), false, Astral);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, From - FVector(0.f, 0.f, 5000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

void AAstralWildlifeController::EnterFlightPhase(EAstralFlightPhase NewPhase, AAstralCharacter* Astral)
{
	UE_LOG(LogAstral_Wilds, Display, TEXT("%s flight: %s -> %s"), *Astral->GetName(), *UEnum::GetValueAsString(FlightPhase), *UEnum::GetValueAsString(NewPhase));
	const FAstralFlightTuning& F = Astral->SpeciesData->Flight;
	UCharacterMovementComponent* Move = Astral->GetCharacterMovement();
	FlightPhase = NewPhase;
	FlightPhaseTime = 0.f;
	bTouchedDown = false;

	switch (NewPhase)
	{
	case EAstralFlightPhase::Grounded:
		FlightRestTime = FMath::FRandRange(F.GroundTimeMin, F.GroundTimeMax);
		Move->MaxWalkSpeed = FlyerWalkSpeed;
		ModeTime = 0.f;   // a short look round before it walks anywhere
		WanderPause = FMath::FRandRange(1.f, 2.5f);
		break;

	case EAstralFlightPhase::TakingOff:
		// Raven take-off: a crouch-and-leap with a big first wingbeat, then a
		// shallow climb away.
		StopMovement();
		Move->SetMovementMode(MOVE_Flying);
		Move->MaxFlySpeed = F.CruiseSpeed;
		Move->Velocity += FVector(0.f, 0.f, 300.f);
		break;

	case EAstralFlightPhase::Airborne:
		FlightRestTime = FMath::FRandRange(F.AirTimeMin, F.AirTimeMax);
		OrbitSign = FMath::RandBool() ? 1.f : -1.f;
		break;

	case EAstralFlightPhase::Landing:
	{
		// Somewhere walkable near home (it rests on its own patch).
		FVector Spot;
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		FNavLocation Nav;
		if (PickPoint(HomeLocation, Tuning.RoamRadius, UGameplayStatics::GetPlayerPawn(this, 0), /*bFarthestFromPlayer*/ false, Spot))
		{
			LandingSpot = Spot;
		}
		else if (NavSys && NavSys->ProjectPointToNavigation(HomeLocation, Nav, FVector(500.f, 500.f, 2000.f)))
		{
			LandingSpot = Nav.Location;
		}
		else
		{
			LandingSpot = HomeLocation;
		}
		break;
	}
	}
}

void AAstralWildlifeController::SteerFlight(AAstralCharacter* Astral, const APawn* Player, float DeltaSeconds)
{
	const FAstralFlightTuning& F = Astral->SpeciesData->Flight;
	UCharacterMovementComponent* Move = Astral->GetCharacterMovement();
	const FVector Here = Astral->GetActorLocation();
	const float Half = Astral->GetSimpleCollisionHalfHeight();
	float GroundZ = 0.f;
	const float GroundRef = GroundBelow(Astral, GroundZ) ? GroundZ : Here.Z - Half - F.CruiseHeight;

	if (bTouchedDown)
	{
		return;   // dropping onto its feet; the next decision makes it Grounded
	}
	if (!Move->IsFlying())
	{
		Move->SetMovementMode(MOVE_Flying);   // e.g. knocked into falling by a collision
	}

	FVector Target = Here;
	float DesiredZ = Here.Z;
	float Speed = F.CruiseSpeed;
	switch (FlightPhase)
	{
	case EAstralFlightPhase::TakingOff:
	{
		Move->MaxFlySpeed = F.CruiseSpeed * 0.8f;
		const FVector Fwd = Astral->GetActorForwardVector().GetSafeNormal2D();
		Astral->AddMovementInput((Fwd + FVector(0.f, 0.f, 0.9f)).GetSafeNormal());
		return;
	}

	case EAstralFlightPhase::Landing:
	{
		// Eagle landing: a descending glide down a ~24 deg slope, slowing
		// into the flare over the last few metres, then touchdown.
		const float Dist = FVector::Dist2D(Here, LandingSpot);
		Target = LandingSpot;
		DesiredZ = LandingSpot.Z + Half + GlideSlopeHeight(Dist, F.CruiseHeight);
		Speed = FMath::Lerp(140.f, F.CruiseSpeed, FMath::Clamp(Dist / 900.f, 0.f, 1.f));
		const float FootClearance = Here.Z - Half - FMath::Max(GroundRef, LandingSpot.Z);
		if ((Dist < 150.f && FootClearance < 70.f) || FlightPhaseTime > 25.f)
		{
			Move->SetMovementMode(MOVE_Falling);   // drops the last bit onto its feet, then walks
			bTouchedDown = true;
			return;
		}
		break;
	}

	case EAstralFlightPhase::Airborne:
	default:
		if (Mode == EAstralWildlifeMode::Chase && Player)
		{
			// Swoops low over the player; the arc turning carries it past and
			// round for another pass.
			Target = Player->GetActorLocation();
			DesiredZ = Target.Z + 150.f + Half;
			Speed = F.ChaseSpeed;
		}
		else if (Mode == EAstralWildlifeMode::Flee && Player)
		{
			Target = Here + (Here - Player->GetActorLocation()).GetSafeNormal2D() * 1500.f;
			DesiredZ = GroundRef + Half + F.CruiseHeight * 1.3f;
			Speed = F.FleeSpeed;
		}
		else
		{
			// Soaring circles round home, with a slow rise and fall.
			Target = OrbitTarget(Here, HomeLocation, F.SoarRadius, OrbitSign);
			DesiredZ = GroundRef + Half + F.CruiseHeight + 80.f * FMath::Sin(FlightPhaseTime * 0.5f);
		}
		break;
	}

	Move->MaxFlySpeed = Speed;
	FVector Dir = FVector(Target.X - Here.X, Target.Y - Here.Y, 0.f).GetSafeNormal();
	if (Dir.IsZero())
	{
		Dir = Astral->GetActorForwardVector().GetSafeNormal2D();
	}
	// Gentle height corrections (a hard clamp made it porpoise: climb, overshoot, dive).
	float Vertical = FMath::Clamp((DesiredZ - Here.Z) / 500.f - Move->Velocity.Z / 800.f, -0.5f, 0.6f);

	// Something solid ahead (a wall, the central block): pull up and head for home.
	FHitResult Hit;
	const FVector Ahead = Move->Velocity.GetSafeNormal2D().IsZero() ? Dir : Move->Velocity.GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AstralFlightAhead), false, Astral);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Here, Here + Ahead * 450.f, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		Vertical = 1.f;
		Dir = (Dir + (HomeLocation - Here).GetSafeNormal2D()).GetSafeNormal();
	}
	Astral->AddMovementInput((Dir + FVector(0.f, 0.f, Vertical)).GetSafeNormal());
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

	const bool bWasFleeing = Mode == EAstralWildlifeMode::Flee;
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
	// A raptor on the ground ambles a few steps between flights.
	if (NewMode == EAstralWildlifeMode::Wander && Astral->SpeciesData && Astral->SpeciesData->bCanFly)
	{
		Speed = FlyerWalkSpeed;
	}
	if (UCharacterMovementComponent* Movement = Astral->GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = Speed;
	}

	// A flee that ends doesn't brake to a dead stop: the Astral eases down to
	// a walk and carries on a few metres the way it was going, then pauses
	// as usual (it used to go 450 -> 0 cm/s in about a second).
	if (bWasFleeing && NewMode == EAstralWildlifeMode::Wander && !Astral->GetCharacterMovement()->IsFlying())
	{
		const FVector Dir = Astral->GetVelocity().GetSafeNormal2D();
		UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
		FNavLocation RunOut;
		if (!Dir.IsZero() && NavSys && NavSys->ProjectPointToNavigation(Astral->GetActorLocation() + Dir * Tuning.FleeRunOut, RunOut))
		{
			MoveToLocation(RunOut.Location, 15.f);
		}
	}
}

bool AAstralWildlifeController::PickPoint(const FVector& Origin, float Radius, const APawn* Player, bool bFarthestFromPlayer, FVector& OutPoint) const
{
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys)
	{
		return false;
	}

	// Wandering prefers points ahead of the Astral, so it sets off forward and
	// curves rather than spinning on the spot first (TJ, 2026-10-08: starts
	// and turns read as unnatural). Points behind are only a fallback.
	const APawn* Self = GetPawn();
	const FVector Facing = Self ? Self->GetActorForwardVector().GetSafeNormal2D() : FVector::ZeroVector;
	constexpr float AheadCos = 0.26f;   // within ~75 deg of facing
	float BestAhead = -2.f;

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
			// Wandering: any point outside the player's alert range, the
			// first one ahead if there is one, else the most nearly ahead.
			const FVector To = (Candidate.Location - (Self ? Self->GetActorLocation() : Origin)).GetSafeNormal2D();
			const float Ahead = Facing.IsZero() ? 1.f : FVector::DotProduct(Facing, To);
			if (Ahead >= AheadCos)
			{
				OutPoint = Candidate.Location;
				return true;
			}
			if (Ahead > BestAhead)
			{
				BestAhead = Ahead;
				OutPoint = Candidate.Location;
				bFound = true;
			}
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
