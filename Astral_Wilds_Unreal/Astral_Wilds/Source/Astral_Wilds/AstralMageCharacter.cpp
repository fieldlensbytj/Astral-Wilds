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

void AAstralMageCharacter::BeginBattle(const FString& PrimaryId, const FString& PrimaryName,
	const FString& CompanionId, const FString& CompanionName, int32 OpponentDamage)
{
	if (BattleEngine)
	{
		return;
	}

	ActiveParty[0] = UAstralBattleEngine::FindFirstEligibleParty(Party, 0);
	ActiveParty[1] = UAstralBattleEngine::FindFirstEligibleParty(Party, ActiveParty[0] + 1);
	if (ActiveParty[0] < 0)
	{
		// No healthy Astrals - nothing to fight with. Caller should have
		// checked this and routed to a defeat/recovery flow instead.
		return;
	}
	if (ActiveParty[1] < 0)
	{
		// Only one eligible Astral - still allowed to fight, the second slot
		// simply stays empty (GetActiveParty returns nullptr for it).
		ActiveParty[1] = ActiveParty[0];
	}

	SelectedActiveSlot = 0;
	SelectedTargetSlot = 0;

	BattleEngine = NewObject<UAstralBattleEngine>(this);
	BattleEngine->Initialize(&Party, ActiveParty, PrimaryId, PrimaryName, CompanionId, CompanionName, OpponentDamage);
}

void AAstralMageCharacter::EndBattle()
{
	BattleEngine = nullptr;
}

void AAstralMageCharacter::SelectActiveSlot(int32 Slot)
{
	if (BattleEngine && Slot >= 0 && Slot < 2)
	{
		SelectedActiveSlot = Slot;
	}
}

void AAstralMageCharacter::SelectTargetSlot(int32 Slot)
{
	if (BattleEngine && Slot >= 0 && Slot < 2)
	{
		SelectedTargetSlot = Slot;
	}
}

void AAstralMageCharacter::DoAttack()
{
	if (IsWeavingResonance() || !BattleEngine)
	{
		return;
	}

	int32 Damage = 0;
	const EAstralActionOutcome Outcome = BattleEngine->QueueAttack(SelectedActiveSlot, SelectedTargetSlot, Damage);
	OnActionResolved.Broadcast(Outcome, Damage);

	if (Outcome != EAstralActionOutcome::Applied)
	{
		return;
	}

	if (BattleEngine->AllOpponentsDefeated())
	{
		EndBattle();
		OnBattleEnded.Broadcast(true);
		return;
	}

	FinishRoundIfReady();
}

void AAstralMageCharacter::DoArcBurst()
{
	if (IsWeavingResonance() || !BattleEngine)
	{
		return;
	}

	int32 TargetsHit = 0;
	const EAstralActionOutcome Outcome = BattleEngine->QueueArcBurst(SelectedActiveSlot, TargetsHit);
	OnActionResolved.Broadcast(Outcome, UAstralCombatRules::ArcBurstDamagePerTarget * TargetsHit);

	if (Outcome != EAstralActionOutcome::Applied)
	{
		return;
	}

	if (BattleEngine->AllOpponentsDefeated())
	{
		EndBattle();
		OnBattleEnded.Broadcast(true);
		return;
	}

	FinishRoundIfReady();
}

void AAstralMageCharacter::DoGuardStart()
{
	if (bIsGuarding)
	{
		return;
	}
	bIsGuarding = true;
	OnGuardChanged.Broadcast(bIsGuarding);

	if (IsWeavingResonance() || !BattleEngine)
	{
		return;
	}

	const EAstralActionOutcome Outcome = BattleEngine->QueueGuard(SelectedActiveSlot);
	OnActionResolved.Broadcast(Outcome, 0);
	if (Outcome == EAstralActionOutcome::Applied)
	{
		FinishRoundIfReady();
	}
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

void AAstralMageCharacter::FinishRoundIfReady()
{
	if (!BattleEngine)
	{
		return;
	}

	for (int32 Slot = 0; Slot < 2; Slot++)
	{
		if (ActiveParty[Slot] < 0 || ActiveParty[Slot] >= Party.Num())
		{
			continue;
		}
		const FAstralCombatant& Member = Party[ActiveParty[Slot]];
		if (!Member.bDefeated && !BattleEngine->HasActed(Slot))
		{
			SelectedActiveSlot = Slot;
			return;
		}
	}

	const FAstralRoundOutcome Result = BattleEngine->TryFinishRound();
	if (!Result.bResolved)
	{
		return;
	}

	OnRoundResolved.Broadcast(Result);

	if (Result.bPlayerDefeat)
	{
		EndBattle();
		OnBattleEnded.Broadcast(false);
	}
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
