#include "AstralMageCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "AstralCombatRules.h"
#include "Astral_Wilds.h"
#include "GameFramework/Controller.h"
#include "Engine/LocalPlayer.h"

AAstralMageCharacter::AAstralMageCharacter()
{
	ResonanceWeave = CreateDefaultSubobject<UAstralResonanceWeaveComponent>(TEXT("ResonanceWeave"));
}

void AAstralMageCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Base class wires Move/Look/Jump (keyboard+mouse+gamepad, via the
	// project's Enhanced Input Mapping Contexts) before we add the Mage's
	// own actions on top.
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (ResonanceWeave)
	{
		ResonanceWeave->OnWeaveResult.AddDynamic(this, &AAstralMageCharacter::OnResonanceWeaveResult);
	}

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleAttack);
		EnhancedInputComponent->BindAction(ArcBurstAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleArcBurst);
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleGuardStarted);
		EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Completed, this, &AAstralMageCharacter::HandleGuardCompleted);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleInteract);

		// Bound up front so they're live the moment ResonanceWeaveMappingContext
		// is pushed; they're inert (component ignores input) whenever no weave
		// is active, since Enhanced Input still delivers events for a mapped
		// action even outside an active context switch window.
		EnhancedInputComponent->BindAction(WeaveAlignmentAction, ETriggerEvent::Triggered, this, &AAstralMageCharacter::HandleWeaveAlignment);
		EnhancedInputComponent->BindAction(ChannelAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleChannelStarted);
		EnhancedInputComponent->BindAction(ChannelAction, ETriggerEvent::Completed, this, &AAstralMageCharacter::HandleChannelCompleted);
		EnhancedInputComponent->BindAction(HarmonizeAction, ETriggerEvent::Started, this, &AAstralMageCharacter::HandleHarmonize);
	}
	else
	{
		UE_LOG(LogAstral_Wilds, Error, TEXT("'%s' Failed to find an Enhanced Input component for Astral actions! Assign AttackAction/ArcBurstAction/GuardAction/InteractAction/WeaveAlignmentAction/ChannelAction/HarmonizeAction on this character (or its Blueprint) to Input Action assets."), *GetNameSafe(this));
	}
}

void AAstralMageCharacter::HandleAttack(const FInputActionValue& Value)
{
	DoAttack();
}

void AAstralMageCharacter::HandleArcBurst(const FInputActionValue& Value)
{
	DoArcBurst();
}

void AAstralMageCharacter::HandleGuardStarted(const FInputActionValue& Value)
{
	DoGuardStart();
}

void AAstralMageCharacter::HandleGuardCompleted(const FInputActionValue& Value)
{
	DoGuardEnd();
}

void AAstralMageCharacter::HandleInteract(const FInputActionValue& Value)
{
	DoInteract();
}

void AAstralMageCharacter::HandleWeaveAlignment(const FInputActionValue& Value)
{
	if (ResonanceWeave && ResonanceWeave->IsWeaveActive())
	{
		ResonanceWeave->SetAlignmentInput(Value.Get<FVector2D>());
	}
}

void AAstralMageCharacter::HandleChannelStarted(const FInputActionValue& Value)
{
	if (ResonanceWeave && ResonanceWeave->IsWeaveActive())
	{
		ResonanceWeave->SetChanneling(true);
	}
}

void AAstralMageCharacter::HandleChannelCompleted(const FInputActionValue& Value)
{
	if (ResonanceWeave)
	{
		ResonanceWeave->SetChanneling(false);
	}
}

void AAstralMageCharacter::HandleHarmonize(const FInputActionValue& Value)
{
	if (ResonanceWeave && ResonanceWeave->IsWeaveActive())
	{
		ResonanceWeave->RespondToHarmonize();
	}
}

void AAstralMageCharacter::OnResonanceWeaveResult(EAstralWeaveResult Result)
{
	// The weave has ended one way or another - hand control of Look back to
	// the default mapping context.
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (ResonanceWeaveMappingContext)
				{
					Subsystem->RemoveMappingContext(ResonanceWeaveMappingContext);
				}
			}
		}
	}

	// TODO: route Result into the wild-encounter system once it exists -
	// Succeeded should complete the bond and add the Astral to the party or
	// reserve; Fled/TurnedHostile/MayRetry should be relayed to that Astral's
	// AI so it reacts according to its own temperament, per the Canon Bible.
}

void AAstralMageCharacter::DoAttack()
{
	if (IsWeavingResonance())
	{
		return;
	}

	// TODO: resolve against the currently targeted opponent Astral once the
	// battle-manager/targeting system exists. For now this establishes the
	// entry point and broadcasts the base attack damage so HUD/VFX/animation
	// can already hook in.
	const int32 Damage = UAstralCombatRules::BasicAttackDamage;
	OnActionResolved.Broadcast(EAstralActionOutcome::Applied, Damage);
}

void AAstralMageCharacter::DoArcBurst()
{
	if (IsWeavingResonance())
	{
		return;
	}

	// TODO: apply UAstralCombatRules::ArcBurstDamagePerTarget to each living
	// active opponent once targeting exists.
	OnActionResolved.Broadcast(EAstralActionOutcome::Applied, UAstralCombatRules::ArcBurstDamagePerTarget);
}

void AAstralMageCharacter::DoGuardStart()
{
	if (bIsGuarding)
	{
		return;
	}
	bIsGuarding = true;
	OnGuardChanged.Broadcast(bIsGuarding);
}

void AAstralMageCharacter::DoGuardEnd()
{
	if (!bIsGuarding)
	{
		return;
	}
	bIsGuarding = false;
	OnGuardChanged.Broadcast(bIsGuarding);
}

void AAstralMageCharacter::DoInteract()
{
	if (IsWeavingResonance() || !ResonanceWeave)
	{
		return;
	}

	// TODO: trace forward for a receptive wild Astral and pull its real
	// FAstralWeaveTemperament + bUseOldConcordance (once Old Concordance is
	// unlocked, per the Canon Bible's Elyndra revelation) instead of this
	// placeholder default. If nothing receptive is in front of the Mage,
	// this should fall back to a normal world interact instead.
	const FAstralWeaveTemperament DefaultTemperament;
	ResonanceWeave->BeginWeave(DefaultTemperament, /*bUseOldConcordance=*/ false);

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (const ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				if (ResonanceWeaveMappingContext)
				{
					Subsystem->AddMappingContext(ResonanceWeaveMappingContext, ResonanceWeaveMappingPriority);
				}
			}
		}
	}
}
