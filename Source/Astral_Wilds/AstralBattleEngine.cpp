#include "AstralBattleEngine.h"
#include "AstralCombatRules.h"

void UAstralBattleEngine::Initialize(TArray<FAstralCombatant>* InParty, const int32 InActiveParty[2],
	const FString& PrimaryId, const FString& PrimaryName,
	const FString& CompanionId, const FString& CompanionName,
	int32 InOpponentDamage)
{
	Party = InParty;
	ActiveParty[0] = InActiveParty[0];
	ActiveParty[1] = InActiveParty[1];
	OpponentDamage = FMath::Max(1, InOpponentDamage);

	Opponents[0] = FAstralCombatant();
	Opponents[0].Id = PrimaryId;
	Opponents[0].DisplayName = PrimaryName;

	Opponents[1] = FAstralCombatant();
	Opponents[1].Id = CompanionId;
	Opponents[1].DisplayName = CompanionName;

	bActed[0] = bActed[1] = false;
	bGuarded[0] = bGuarded[1] = false;
}

FAstralCombatant UAstralBattleEngine::GetOpponent(int32 Slot) const
{
	if (Slot < 0 || Slot > 1)
	{
		return FAstralCombatant();
	}
	return Opponents[Slot];
}

bool UAstralBattleEngine::HasActed(int32 Slot) const
{
	return Slot >= 0 && Slot < 2 && bActed[Slot];
}

bool UAstralBattleEngine::IsGuarded(int32 Slot) const
{
	return Slot >= 0 && Slot < 2 && bGuarded[Slot];
}

FAstralCombatant* UAstralBattleEngine::GetActiveParty(int32 Slot) const
{
	if (!Party || Slot < 0 || Slot > 1)
	{
		return nullptr;
	}
	const int32 PartyIndex = ActiveParty[Slot];
	if (PartyIndex < 0 || PartyIndex >= Party->Num())
	{
		return nullptr;
	}
	return &(*Party)[PartyIndex];
}

bool UAstralBattleEngine::AllOpponentsDefeated() const
{
	return Opponents[0].bDefeated && Opponents[1].bDefeated;
}

EAstralActionOutcome UAstralBattleEngine::QueueAttack(int32 ActorSlot, int32 TargetSlot, int32& OutDamageDealt)
{
	OutDamageDealt = 0;
	FAstralCombatant* Actor = GetActiveParty(ActorSlot);
	if (TargetSlot < 0 || TargetSlot > 1)
	{
		return EAstralActionOutcome::Invalid;
	}
	FAstralCombatant& Target = Opponents[TargetSlot];

	if (!Actor || Actor->bDefeated || Target.bDefeated)
	{
		return EAstralActionOutcome::Invalid;
	}
	if (bActed[ActorSlot])
	{
		return EAstralActionOutcome::SlotAlreadyActed;
	}

	bActed[ActorSlot] = true;
	OutDamageDealt = UAstralCombatRules::BasicAttackDamage;
	ApplyDamageToOpponent(TargetSlot, OutDamageDealt);
	return EAstralActionOutcome::Applied;
}

EAstralActionOutcome UAstralBattleEngine::QueueArcBurst(int32 ActorSlot, int32& OutTargetsHit)
{
	OutTargetsHit = 0;
	FAstralCombatant* Actor = GetActiveParty(ActorSlot);
	if (!Actor || Actor->bDefeated)
	{
		return EAstralActionOutcome::Invalid;
	}
	if (bActed[ActorSlot])
	{
		return EAstralActionOutcome::SlotAlreadyActed;
	}

	bActed[ActorSlot] = true;
	for (int32 Slot = 0; Slot < 2; Slot++)
	{
		if (Opponents[Slot].bDefeated)
		{
			continue;
		}
		ApplyDamageToOpponent(Slot, UAstralCombatRules::ArcBurstDamagePerTarget);
		OutTargetsHit++;
	}
	return EAstralActionOutcome::Applied;
}

EAstralActionOutcome UAstralBattleEngine::QueueGuard(int32 ActorSlot)
{
	FAstralCombatant* Actor = GetActiveParty(ActorSlot);
	if (!Actor || Actor->bDefeated)
	{
		return EAstralActionOutcome::Invalid;
	}
	if (bActed[ActorSlot])
	{
		return EAstralActionOutcome::SlotAlreadyActed;
	}

	bActed[ActorSlot] = true;
	bGuarded[ActorSlot] = true;
	return EAstralActionOutcome::Applied;
}

void UAstralBattleEngine::MarkActed(int32 Slot)
{
	if (Slot >= 0 && Slot < 2)
	{
		bActed[Slot] = true;
	}
}

void UAstralBattleEngine::ApplyDamageToOpponent(int32 Slot, int32 Damage)
{
	if (Slot < 0 || Slot > 1)
	{
		return;
	}
	FAstralCombatant& Target = Opponents[Slot];
	if (Target.bDefeated || Damage <= 0)
	{
		return;
	}
	Target.Hp = FMath::Max(0, Target.Hp - Damage);
	if (Target.Hp == 0)
	{
		Target.bDefeated = true;
	}
}

FAstralRoundOutcome UAstralBattleEngine::TryFinishRound()
{
	FAstralRoundOutcome Outcome;

	for (int32 Slot = 0; Slot < 2; Slot++)
	{
		FAstralCombatant* Member = GetActiveParty(Slot);
		if (Member && !Member->bDefeated && !bActed[Slot])
		{
			Outcome.bResolved = false;
			return Outcome;
		}
	}

	Outcome.Summary = ResolveOpponentActions();
	bActed[0] = bActed[1] = false;

	Outcome.bResolved = true;
	Outcome.bPlayerDefeat = Party ? AllDefeated(*Party) : false;
	return Outcome;
}

FString UAstralBattleEngine::ResolveOpponentActions()
{
	TArray<FString> Results;

	for (int32 i = 0; i < 2; i++)
	{
		FAstralCombatant& Enemy = Opponents[i];
		int32 TargetSlot = i % 2;
		FAstralCombatant* Target = GetActiveParty(TargetSlot);
		if (!Target || Target->bDefeated)
		{
			TargetSlot = (i + 1) % 2;
			Target = GetActiveParty(TargetSlot);
		}
		if (Enemy.bDefeated || !Target || Target->bDefeated)
		{
			continue;
		}

		const bool bWasGuarded = bGuarded[TargetSlot];
		const int32 Damage = UAstralCombatRules::ResolveIncomingDamage(OpponentDamage, bWasGuarded);
		Target->Hp = FMath::Max(0, Target->Hp - Damage);
		const bool bFainted = (Target->Hp == 0);
		if (bFainted)
		{
			Target->bDefeated = true;
		}

		Results.Add(FString::Printf(TEXT("%s%s took %d%s"),
			*Target->DisplayName,
			bWasGuarded ? TEXT(" guarded and") : TEXT(""),
			Damage,
			bFainted ? TEXT(" and fainted") : TEXT("")));
	}

	bGuarded[0] = bGuarded[1] = false;
	return Results.Num() == 0 ? TEXT("no counterattacks landed.") : FString::Join(Results, TEXT("; ")) + TEXT(".");
}

int32 UAstralBattleEngine::FindFirstEligibleParty(const TArray<FAstralCombatant>& Party, int32 Start)
{
	for (int32 i = FMath::Max(0, Start); i < Party.Num(); i++)
	{
		if (!Party[i].bDefeated)
		{
			return i;
		}
	}
	return -1;
}

bool UAstralBattleEngine::AllDefeated(const TArray<FAstralCombatant>& Party)
{
	for (int32 i = 0; i < Party.Num(); i++)
	{
		if (!Party[i].bDefeated)
		{
			return false;
		}
	}
	return true;
}
