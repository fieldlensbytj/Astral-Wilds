// Astral Wilds - see AstralWildlifeStateTreeUtility.h.
#include "AstralWildlifeStateTreeUtility.h"
#include "StateTreeExecutionContext.h"
#include "AstralCharacter.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

namespace AstralWildlifeStateTreeUtility_Private
{
	static AAIController* GetAIController(AAstralCharacter* Character)
	{
		return Character ? Cast<AAIController>(Character->GetController()) : nullptr;
	}

	static APawn* GetLocalPlayerPawn(const UObject* WorldContextObject)
	{
		return UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
	}
}
using namespace AstralWildlifeStateTreeUtility_Private;

bool FStateTreeAstralArchetypeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character || !InstanceData.Character->SpeciesData)
	{
		return false;
	}

	return InstanceData.Character->SpeciesData->AIArchetype == InstanceData.ExpectedArchetype;
}

#if WITH_EDITOR
FText FStateTreeAstralArchetypeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Astral Archetype Is</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

bool FStateTreeAstralPlayerInRangeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character)
	{
		return false;
	}

	const APawn* Player = GetLocalPlayerPawn(InstanceData.Character);
	if (!Player)
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(InstanceData.Character->GetActorLocation(), Player->GetActorLocation());
	const bool bWithinRange = DistSq <= FMath::Square(InstanceData.Range);

	return InstanceData.bMustBeOutOfRange ? !bWithinRange : bWithinRange;
}

#if WITH_EDITOR
FText FStateTreeAstralPlayerInRangeCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Astral Player In Range</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeAstralRoamTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character)
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* Controller = GetAIController(InstanceData.Character);
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(InstanceData.Character->GetWorld());
	if (!Controller || !NavSys)
	{
		return EStateTreeRunStatus::Failed;
	}

	FNavLocation RandomLocation;
	if (!NavSys->GetRandomReachablePointInRadius(InstanceData.Character->GetActorLocation(), InstanceData.RoamRadius, RandomLocation))
	{
		return EStateTreeRunStatus::Failed;
	}

	Controller->MoveToLocation(RandomLocation.Location, InstanceData.AcceptanceRadius);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeAstralRoamTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AAIController* Controller = GetAIController(InstanceData.Character);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	return Controller->GetMoveStatus() == EPathFollowingStatus::Idle ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeAstralRoamTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Roam</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeAstralFleeTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character)
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* Controller = GetAIController(InstanceData.Character);
	const APawn* Player = GetLocalPlayerPawn(InstanceData.Character);
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(InstanceData.Character->GetWorld());
	if (!Controller || !Player || !NavSys)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector AwayDir = (InstanceData.Character->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
	const FVector DesiredPoint = InstanceData.Character->GetActorLocation() + AwayDir * InstanceData.FleeDistance;

	FNavLocation ProjectedLocation;
	const FVector Destination = NavSys->ProjectPointToNavigation(DesiredPoint, ProjectedLocation) ? ProjectedLocation.Location : DesiredPoint;

	Controller->MoveToLocation(Destination);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeAstralFleeTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	AAIController* Controller = GetAIController(InstanceData.Character);
	if (!Controller)
	{
		return EStateTreeRunStatus::Failed;
	}

	return Controller->GetMoveStatus() == EPathFollowingStatus::Idle ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeAstralFleeTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Flee</b>");
}
#endif // WITH_EDITOR

////////////////////////////////////////////////////////////////////

EStateTreeRunStatus FStateTreeAstralAggroChaseTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character)
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* Controller = GetAIController(InstanceData.Character);
	const APawn* Player = GetLocalPlayerPawn(InstanceData.Character);
	if (!Controller || !Player)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.TimeSinceLastRepath = 0.f;
	Controller->MoveToLocation(Player->GetActorLocation(), InstanceData.AcceptanceRadius);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeAstralAggroChaseTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.Character)
	{
		return EStateTreeRunStatus::Failed;
	}

	const APawn* Player = GetLocalPlayerPawn(InstanceData.Character);
	if (!Player)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Close enough - succeed. Whatever transitions off this state's success is
	// the natural hook for routing into combat once task 3 wires it; this task
	// only handles the chase itself.
	const float DistSq = FVector::DistSquared(InstanceData.Character->GetActorLocation(), Player->GetActorLocation());
	if (DistSq <= FMath::Square(InstanceData.AcceptanceRadius))
	{
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.TimeSinceLastRepath += DeltaTime;
	if (InstanceData.TimeSinceLastRepath >= InstanceData.RepathInterval)
	{
		InstanceData.TimeSinceLastRepath = 0.f;
		if (AAIController* Controller = GetAIController(InstanceData.Character))
		{
			Controller->MoveToLocation(Player->GetActorLocation(), InstanceData.AcceptanceRadius);
		}
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FStateTreeAstralAggroChaseTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting /*= EStateTreeNodeFormatting::Text*/) const
{
	return FText::FromString("<b>Aggro Chase</b>");
}
#endif // WITH_EDITOR
