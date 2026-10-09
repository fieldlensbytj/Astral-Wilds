// Astral Wilds - see AstralCharacter.h.
#include "AstralCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "AstralWildlifeController.h"
#include "AstralLocomotionAnimInstance.h"
#include "AstralMovementComponent.h"

AAstralCharacter::AAstralCharacter(const FObjectInitializer& ObjectInitializer)
	// Turns carve arcs at speed (see AstralMovementComponent.h).
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UAstralMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Wild Astrals roam under AI control by default (see AstralWildlifeController.h);
	// a future bonded/party representation can override this once that system exists.
	AIControllerClass = AAstralWildlifeController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	// Animal-like momentum instead of the engine's near-instant defaults
	// (360 deg/s turns, 2048 cm/s^2): starts, stops and turns read as motion
	// the locomotion blend can follow rather than snaps.
	GetCharacterMovement()->RotationRate = FRotator(0.f, 240.f, 0.f);
	GetCharacterMovement()->MaxAcceleration = 900.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 600.f;
	// Default braking also applies ground friction (8 x 2), which halves speed
	// every frame - a 450 cm/s flee stopped in 0.1s. Brake on deceleration instead.
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->BrakingFriction = 0.5f;
	// AI paths: accelerate/brake like player input instead of snapping velocity,
	// and ease to a stop over the last 1.5m of a path.
	FNavMovementProperties* NavMove = GetCharacterMovement()->GetNavMovementProperties();
	NavMove->bUseAccelerationForPaths = true;
	NavMove->bUseFixedBrakingDistanceForPaths = true;
	NavMove->FixedPathBrakingDistance = 150.f;

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(RootComponent);
	// PLACEHOLDER: the engine ships no basic-shape capsule, so a cylinder stands in.
	// Scale approximates the default capsule collision (radius 34 / half-height 88)
	// against the basic shape's 100x100x100 bounds. Adjust per real art.
	PlaceholderMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshAsset.Succeeded())
	{
		PlaceholderShape = CylinderMeshAsset.Object;
		PlaceholderMesh->SetStaticMesh(PlaceholderShape);
	}

	GetMesh()->SetVisibility(false);

	// Mirrors AWildAstralEncounter's InteractSphere exactly (same profile) so
	// AAstralMageCharacter::FindReceptiveWildAstral()'s ECC_WorldDynamic overlap
	// query detects this actor the same way it detected AWildAstralEncounter.
	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->SetupAttachment(RootComponent);
	InteractSphere->InitSphereRadius(InteractRadius);
	InteractSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	RecomputeStatsForLevel();
	ApplySpeciesVisuals();
}

void AAstralCharacter::RecomputeStatsForLevel()
{
	if (SpeciesData)
	{
		CurrentStats = SpeciesData->ComputeStatsForLevel(Level);
	}
	else
	{
		CurrentStats = FAstralBaseStats();
		CurrentStats.HP = 0;
		CurrentStats.Attack = 0;
		CurrentStats.Defense = 0;
		CurrentStats.Speed = 0;
	}

	// No damage/persistence system yet - recomputing always presents full health.
	CurrentHP = CurrentStats.HP;
}

int32 AAstralCharacter::GetXPToNextLevel() const
{
	return SpeciesData ? SpeciesData->ComputeXPRequiredForLevel(Level + 1) : 0;
}

FAstralCombatant AAstralCharacter::ToCombatant() const
{
	FAstralCombatant Combatant;
	Combatant.Id = GetName(); // TODO: replace with a persistent GUID once a save system exists
	Combatant.DisplayName = SpeciesData ? SpeciesData->SpeciesName.ToString() : TEXT("Unknown Astral");
	Combatant.PrimaryEssence = SpeciesData ? SpeciesData->PrimaryType : EAstralEssence::Ember;
	Combatant.bHasSecondaryEssence = SpeciesData && SpeciesData->bHasSecondaryType;
	Combatant.SecondaryEssence = SpeciesData ? SpeciesData->SecondaryType : EAstralEssence::Ember;
	Combatant.Hp = CurrentHP;
	Combatant.MaxHp = CurrentStats.HP;
	Combatant.bDefeated = CurrentHP <= 0;
	Combatant.SpeciesData = SpeciesData;
	Combatant.Level = Level;
	Combatant.CurrentXP = CurrentXP;
	Combatant.CurrentStats = CurrentStats;
	return Combatant;
}

void AAstralCharacter::InitFromCombatant(const FAstralCombatant& Combatant)
{
	SpeciesData = Combatant.SpeciesData;
	Level = Combatant.Level;
	CurrentXP = Combatant.CurrentXP;

	RecomputeStatsForLevel(); // full-heals CurrentHP as a side effect; overridden below to preserve carried-over HP
	CurrentHP = FMath::Clamp(Combatant.Hp, 0, CurrentStats.HP);

	ApplySpeciesVisuals();
}

void AAstralCharacter::TryBecomeReceptive()
{
	if (WildState == EAstralWildState::Enraged || WildState == EAstralWildState::Territorial)
	{
		// Aggressive species don't simply calm down on their own - a subclass
		// must implement real behavior (combat/exhaustion, feeding, etc.) and
		// call SetWildState() directly once that condition is satisfied.
		return;
	}

	WildState = EAstralWildState::Receptive;
}

void AAstralCharacter::ApplySpeciesVisuals()
{
	USkeletalMeshComponent* SkeletalMeshComp = GetMesh();

	if (SpeciesData && !SpeciesData->DisplayMesh.IsNull())
	{
		if (USkeletalMesh* NewSkeletalMesh = SpeciesData->DisplayMesh.LoadSynchronous())
		{
			SkeletalMeshComp->SetSkeletalMesh(NewSkeletalMesh);

			// Same auto-fit as static display models: uniform scale to
			// DisplayHeight, centred on the capsule axis, base on the ground.
			const FBox Bounds = NewSkeletalMesh->GetImportedBounds().GetBox();
			const float ModelHeight = Bounds.GetSize().Z;
			const float Scale = ModelHeight > KINDA_SMALL_NUMBER ? SpeciesData->DisplayHeight / ModelHeight : 1.f;
			const FRotator Yaw(0.f, SpeciesData->DisplayYawOffset, 0.f);
			const FVector CenterXY = Yaw.RotateVector(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.f)) * Scale;
			SkeletalMeshComp->SetRelativeScale3D(FVector(Scale));
			SkeletalMeshComp->SetRelativeRotation(Yaw);
			SkeletalMeshComp->SetRelativeLocation(FVector(-CenterXY.X, -CenterXY.Y, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - Bounds.Min.Z * Scale));

			bHasRiggedDisplay = false;
			if (!SpeciesData->AnimClass.IsNull())
			{
				if (UClass* AnimClassPtr = SpeciesData->AnimClass.LoadSynchronous())
				{
					SkeletalMeshComp->SetAnimInstanceClass(AnimClassPtr);
				}
			}
			else
			{
				LoadedIdle = SpeciesData->IdleAnim.LoadSynchronous();
				LoadedWalk = SpeciesData->WalkAnim.LoadSynchronous();
				LoadedRun = SpeciesData->RunAnim.LoadSynchronous();
				LoadedFly = SpeciesData->FlyAnim.LoadSynchronous();
				LoadedGlide = SpeciesData->GlideAnim.LoadSynchronous();
				LoadedFlare = SpeciesData->FlareAnim.LoadSynchronous();
				bHasRiggedDisplay = LoadedIdle || LoadedWalk || LoadedRun;
				if (bHasRiggedDisplay)
				{
					DisplayRestRotation = Yaw;
					BoundLocomotionInstance.Reset();
					SkeletalMeshComp->SetAnimationMode(EAnimationMode::AnimationBlueprint);
					SkeletalMeshComp->SetAnimInstanceClass(UAstralLocomotionAnimInstance::StaticClass());
					BindLocomotionClips();
				}
			}
			bHasStaticDisplay = false;

			SkeletalMeshComp->SetVisibility(true);
			if (PlaceholderMesh)
			{
				PlaceholderMesh->SetVisibility(false);
			}
			return;
		}
	}

	SkeletalMeshComp->SetVisibility(false);
	if (!PlaceholderMesh)
	{
		return;
	}
	PlaceholderMesh->SetVisibility(true);

	UStaticMesh* StaticModel = (SpeciesData && !SpeciesData->DisplayStaticMesh.IsNull()) ? SpeciesData->DisplayStaticMesh.LoadSynchronous() : nullptr;
	if (StaticModel)
	{
		// Auto-fit: uniform scale to DisplayHeight, centred on the capsule
		// axis, base resting at the bottom of the capsule.
		const FBox Bounds = StaticModel->GetBoundingBox();
		const float ModelHeight = Bounds.GetSize().Z;
		const float Scale = ModelHeight > KINDA_SMALL_NUMBER ? SpeciesData->DisplayHeight / ModelHeight : 1.f;
		const FRotator Yaw(0.f, SpeciesData->DisplayYawOffset, 0.f);
		const FVector CenterXY = Yaw.RotateVector(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.f)) * Scale;
		const float CapsuleBottom = -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

		PlaceholderMesh->SetStaticMesh(StaticModel);
		PlaceholderMesh->SetRelativeScale3D(FVector(Scale));
		PlaceholderMesh->SetRelativeRotation(Yaw);
		PlaceholderMesh->SetRelativeLocation(FVector(-CenterXY.X, -CenterXY.Y, CapsuleBottom - Bounds.Min.Z * Scale));
		bHasStaticDisplay = true;
		DisplayRestLocation = PlaceholderMesh->GetRelativeLocation();
		DisplayRestRotation = Yaw;
		DisplayRestScale = Scale;
		BreathTime = FMath::FRandRange(0.f, 10.f); // desync a group's breathing
		return;
	}

	bHasStaticDisplay = false;

	// PLACEHOLDER cylinder - see the constructor for the scale's derivation.
	PlaceholderMesh->SetStaticMesh(PlaceholderShape);
	PlaceholderMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	PlaceholderMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PlaceholderMesh->SetRelativeLocation(FVector::ZeroVector);
}

void AAstralCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	// 0 at rest -> 1 at a full run; smoothed so starts/stops ease.
	SmoothedSpeedAlpha = FMath::FInterpTo(SmoothedSpeedAlpha, FMath::Clamp(Speed / 450.f, 0.f, 1.f), DeltaSeconds, 6.f);

	if (bHasRiggedDisplay)
	{
		// The clips carry gait, bob and breathing; only the turn lean is added here.
		BindLocomotionClips();
		if (bProceduralMotion)
		{
			// Flying: the clips beat the wings; the body banks into turns,
			// pitches with its climb/dive and flares nose-up when slow.
			const bool bFlying = GetCharacterMovement()->IsFlying();
			UpdateTurnLean(DeltaSeconds, bFlying ? 1.f : SmoothedSpeedAlpha);
			float Target = 0.f;
			if (bFlying)
			{
				const FVector V = GetVelocity();
				const float Climb = FMath::RadiansToDegrees(FMath::Atan2(V.Z, FMath::Max(V.Size2D(), 50.f)));
				Target = FMath::Clamp(Climb * 0.7f, -25.f, 25.f) + 8.f * FMath::Clamp(1.f - V.Size2D() / 250.f, 0.f, 1.f);   // the Flare clip carries the nose-up
			}
			FlightPitch = FMath::FInterpTo(FlightPitch, Target, DeltaSeconds, 3.f);
			GetMesh()->SetRelativeRotation(FQuat(FRotator(FlightPitch, 0.f, SmoothedLean)) * FQuat(DisplayRestRotation));
		}
		return;
	}

	if (!bProceduralMotion || !bHasStaticDisplay || !PlaceholderMesh)
	{
		return;
	}

	if (GetCharacterMovement()->IsFlying())
	{
		UpdateFlightPose(DeltaSeconds);
		return;
	}
	FlightPitch = FMath::FInterpTo(FlightPitch, 0.f, DeltaSeconds, 4.f);

	const float MaxSpeed = FMath::Max(GetCharacterMovement()->MaxWalkSpeed, 1.f);

	// Gait: one bob per stride, |sin| gives a footfall-like bounce.
	GaitPhase = FMath::Fmod(GaitPhase + Speed / FMath::Max(GaitStride, 1.f) * PI * DeltaSeconds, 2.f * PI);
	const float Bob = FMath::Abs(FMath::Sin(GaitPhase)) * GaitBobHeight * SmoothedSpeedAlpha;
	const float GaitPitch = FMath::Sin(GaitPhase * 2.f) * 2.5f * SmoothedSpeedAlpha;   // nod with each step
	const float RunPitch = -4.f * SmoothedSpeedAlpha * (Speed / MaxSpeed);              // lean forward when fast

	UpdateTurnLean(DeltaSeconds, SmoothedSpeedAlpha);

	// Breathing: slow chest-like swell, faded out while moving.
	BreathTime += DeltaSeconds;
	const float Breath = FMath::Sin(BreathTime * 2.f * PI * 0.35f) * 0.018f * (1.f - SmoothedSpeedAlpha);

	// Offsets are in actor space; DisplayRestRotation already carries the model's yaw fix.
	PlaceholderMesh->SetRelativeLocation(DisplayRestLocation + FVector(0.f, 0.f, Bob));
	// Compose in actor space (left-multiply): the model's own axes are yawed by DisplayRestRotation.
	PlaceholderMesh->SetRelativeRotation(FQuat(FRotator(GaitPitch + RunPitch + FlightPitch, 0.f, SmoothedLean)) * FQuat(DisplayRestRotation));
	PlaceholderMesh->SetRelativeScale3D(FVector(DisplayRestScale * (1.f - Breath * 0.5f), DisplayRestScale * (1.f - Breath * 0.5f), DisplayRestScale * (1.f + Breath)));
}

void AAstralCharacter::UpdateFlightPose(float DeltaSeconds)
{
	// Bird references (art repo Docs/Design/TurnReference.md): a soaring bird
	// banks hard into its circles and holds the bank; it pitches with its
	// climb or dive; it flaps hard taking off, climbing and flaring to land
	// (body pitched up, slowing), and glides with only a gentle rise and fall
	// in between. The static model has no wings to beat (it needs a wing rig
	// for that), so the flapping shows as the body's quick heave.
	const FVector V = GetVelocity();
	const float Horizontal = V.Size2D();
	UpdateTurnLean(DeltaSeconds, 1.f);

	// Pitch: follow the climb/dive angle, plus a flare (nose up) when slow.
	const float Climb = FMath::RadiansToDegrees(FMath::Atan2(V.Z, FMath::Max(Horizontal, 50.f)));
	const float Flare = 25.f * FMath::Clamp(1.f - Horizontal / 250.f, 0.f, 1.f);
	FlightPitch = FMath::FInterpTo(FlightPitch, FMath::Clamp(Climb * 0.7f, -25.f, 25.f) + Flare, DeltaSeconds, 3.f);

	// Flapping (climbing, slow, or accelerating) vs gliding.
	const bool bFlapping = V.Z > 60.f || Horizontal < 300.f;
	FlapAlpha = FMath::FInterpTo(FlapAlpha, bFlapping ? 1.f : 0.f, DeltaSeconds, 3.f);
	FlapPhase = FMath::Fmod(FlapPhase + DeltaSeconds * 2.f * PI * FMath::Lerp(0.5f, 3.5f, FlapAlpha), 2.f * PI);
	const float Heave = FMath::Sin(FlapPhase) * FMath::Lerp(4.f, 9.f, FlapAlpha);
	const float FlapPitch = FMath::Cos(FlapPhase) * 4.f * FlapAlpha;   // the body rocks with each beat

	PlaceholderMesh->SetRelativeLocation(DisplayRestLocation + FVector(0.f, 0.f, Heave));
	PlaceholderMesh->SetRelativeRotation(FQuat(FRotator(FlightPitch + FlapPitch, 0.f, SmoothedLean)) * FQuat(DisplayRestRotation));
	PlaceholderMesh->SetRelativeScale3D(FVector(DisplayRestScale));
}

void AAstralCharacter::ApplySpeciesMovement()
{
	if (!SpeciesData)
	{
		return;
	}
	MaxTurnLean = SpeciesData->MaxTurnLean;
	UAstralMovementComponent* Move = Cast<UAstralMovementComponent>(GetCharacterMovement());
	if (!Move)
	{
		return;
	}
	Move->MaxTurnAcceleration = SpeciesData->TurnAcceleration;
	if (SpeciesData->bCanFly)
	{
		const FAstralFlightTuning& F = SpeciesData->Flight;
		Move->MaxFlightTurnAcceleration = F.TurnAcceleration;
		Move->MaxFlySpeed = F.CruiseSpeed;
		Move->BrakingDecelerationFlying = 400.f;
		Move->GetNavAgentPropertiesRef().bCanFly = true;
	}
}

void AAstralCharacter::BindLocomotionClips()
{
	UAstralLocomotionAnimInstance* Instance = Cast<UAstralLocomotionAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Instance || !SpeciesData || BoundLocomotionInstance.Get() == Instance)
	{
		return;   // not created yet (e.g. in the constructor), or already bound
	}
	Instance->SetClips(LoadedIdle, LoadedWalk, LoadedRun, SpeciesData->WalkAnimSpeed, SpeciesData->RunAnimSpeed, SpeciesData->IdleSpeedThreshold);
	Instance->HeadLeadMax = SpeciesData->HeadLeadMax;
	Instance->SetFlightClips(LoadedFly, LoadedGlide, LoadedFlare);
	BoundLocomotionInstance = Instance;
}

void AAstralCharacter::UpdateTurnLean(float DeltaSeconds, float SpeedAlpha)
{
	const float Yaw = GetActorRotation().Yaw;
	const float YawRate = FMath::FindDeltaAngleDegrees(LastYaw, Yaw) / DeltaSeconds;
	LastYaw = Yaw;
	// Bank like a runner rounding a base: the lean balances the sideways
	// (centripetal) acceleration, speed x turn rate, so a fast arc banks hard
	// and a slow pivot hardly at all. Half the physical bank angle reads as
	// poised rather than a motorbike; SpeedAlpha keeps walks nearly upright.
	const float Lateral = GetVelocity().Size2D() * FMath::DegreesToRadians(YawRate);
	// A flyer banks at the full physical angle, up to its species' MaxBank.
	const bool bFlying = GetCharacterMovement()->IsFlying();
	const float BankShare = bFlying ? 1.f : 0.5f;
	const float MaxLean = bFlying && SpeciesData ? SpeciesData->Flight.MaxBank : MaxTurnLean;
	const float Bank = -BankShare * FMath::RadiansToDegrees(FMath::Atan2(Lateral, 980.f));
	SmoothedLean = FMath::FInterpTo(SmoothedLean, FMath::Clamp(Bank, -MaxLean, MaxLean) * SpeedAlpha, DeltaSeconds, bFlying ? 2.5f : 5.f);
}

void AAstralCharacter::BeginPlay()
{
	Super::BeginPlay();
	RecomputeStatsForLevel();
	ApplySpeciesVisuals();
	ApplySpeciesMovement();
}

#if WITH_EDITOR
void AAstralCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RecomputeStatsForLevel();
	ApplySpeciesVisuals();
}
#endif
