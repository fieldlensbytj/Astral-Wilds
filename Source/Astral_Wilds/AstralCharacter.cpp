// Astral Wilds - see AstralCharacter.h.
#include "AstralCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "AstralWildlifeController.h"

AAstralCharacter::AAstralCharacter()
{
	// Wild Astrals roam under AI control by default (see AstralWildlifeController.h);
	// a future bonded/party representation can override this once that system exists.
	AIControllerClass = AAstralWildlifeController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

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

			if (!SpeciesData->AnimClass.IsNull())
			{
				if (UClass* AnimClassPtr = SpeciesData->AnimClass.LoadSynchronous())
				{
					SkeletalMeshComp->SetAnimInstanceClass(AnimClassPtr);
				}
			}

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
		return;
	}

	// PLACEHOLDER cylinder - see the constructor for the scale's derivation.
	PlaceholderMesh->SetStaticMesh(PlaceholderShape);
	PlaceholderMesh->SetRelativeScale3D(FVector(0.68f, 0.68f, 1.76f));
	PlaceholderMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PlaceholderMesh->SetRelativeLocation(FVector::ZeroVector);
}

void AAstralCharacter::BeginPlay()
{
	Super::BeginPlay();
	RecomputeStatsForLevel();
	ApplySpeciesVisuals();
}

#if WITH_EDITOR
void AAstralCharacter::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RecomputeStatsForLevel();
	ApplySpeciesVisuals();
}
#endif
