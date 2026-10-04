// Astral Wilds - StateTree building blocks for basic wild-Astral behavior
// (roam/flee/aggro-chase per SpeciesData->AIArchetype). Mirrors the shape of
// Variant_Combat/AI/CombatStateTreeUtility.h exactly (FStateTreeConditionCommonBase/
// FStateTreeTaskCommonBase + paired FInstanceDataType structs) so this follows
// the same convention as the rest of the project's AI, which is StateTree-only
// (no BehaviorTree/Blackboard anywhere in this codebase).
//
// These are deliberately "basic": no EQS, no per-archetype C++ subclasses -
// just enough parameters (ExpectedArchetype, Range, RoamRadius, ...) to be
// combined differently per state in a single shared StateTree graph. Wiring
// that graph is a manual step in the StateTree editor - see the plan/checklist
// this was implemented from for the recommended state/transition layout.
#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "AstralSpeciesData.h"

#include "AstralWildlifeStateTreeUtility.generated.h"

class AAstralCharacter;

/** Instance data for FStateTreeAstralArchetypeCondition. */
USTRUCT()
struct FStateTreeAstralArchetypeConditionInstanceData
{
	GENERATED_BODY()

	/** The wild Astral to check. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAstralCharacter> Character;

	/** Archetype this condition checks for. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	EAstralAIArchetype ExpectedArchetype = EAstralAIArchetype::Docile;
};

/** StateTree condition: true if Character->SpeciesData->AIArchetype == ExpectedArchetype. */
USTRUCT(DisplayName = "Astral Archetype Is")
struct FStateTreeAstralArchetypeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeAstralArchetypeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FStateTreeAstralArchetypeCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

/** Instance data for FStateTreeAstralPlayerInRangeCondition. */
USTRUCT()
struct FStateTreeAstralPlayerInRangeConditionInstanceData
{
	GENERATED_BODY()

	/** The wild Astral to check distance from. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAstralCharacter> Character;

	/** Detection range, in cm. */
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (Units = "cm"))
	float Range = 600.f;

	/** If true, the condition passes when the player is OUTSIDE Range instead of inside it. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	bool bMustBeOutOfRange = false;
};

/** StateTree condition: true if the local player pawn is within (or, inverted, outside) Range of Character. Single-player, matches the rest of the project. */
USTRUCT(DisplayName = "Astral Player In Range")
struct FStateTreeAstralPlayerInRangeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeAstralPlayerInRangeConditionInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	FStateTreeAstralPlayerInRangeCondition() = default;

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

/** Instance data for FStateTreeAstralRoamTask. */
USTRUCT()
struct FStateTreeAstralRoamTaskInstanceData
{
	GENERATED_BODY()

	/** The wild Astral to move. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAstralCharacter> Character;

	/** How far from its current location it may wander in one hop. PLACEHOLDER: not anchored to a fixed spawn origin, so repeated hops can drift over many cycles - acceptable for basic wandering. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "cm"))
	float RoamRadius = 800.f;

	/** Distance to the goal considered "arrived". */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "cm"))
	float AcceptanceRadius = 50.f;
};

/** StateTree task: picks a random reachable point near Character and moves there; succeeds once arrived. */
USTRUCT(meta = (DisplayName = "Astral Roam", Category = "Wildlife"))
struct FStateTreeAstralRoamTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FStateTreeAstralRoamTask()
	{
		bShouldCallTick = true;
	}

	using FInstanceDataType = FStateTreeAstralRoamTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

/** Instance data for FStateTreeAstralFleeTask. */
USTRUCT()
struct FStateTreeAstralFleeTaskInstanceData
{
	GENERATED_BODY()

	/** The wild Astral fleeing. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAstralCharacter> Character;

	/** How far, in the direction away from the player, to flee. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "cm"))
	float FleeDistance = 500.f;
};

/** StateTree task: moves Character directly away from the local player pawn; succeeds once arrived. */
USTRUCT(meta = (DisplayName = "Astral Flee", Category = "Wildlife"))
struct FStateTreeAstralFleeTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FStateTreeAstralFleeTask()
	{
		bShouldCallTick = true;
	}

	using FInstanceDataType = FStateTreeAstralFleeTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};

////////////////////////////////////////////////////////////////////

/** Instance data for FStateTreeAstralAggroChaseTask. */
USTRUCT()
struct FStateTreeAstralAggroChaseTaskInstanceData
{
	GENERATED_BODY()

	/** The wild Astral chasing. */
	UPROPERTY(EditAnywhere, Category = "Context")
	TObjectPtr<AAstralCharacter> Character;

	/** Distance to the player considered "caught up". */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "cm"))
	float AcceptanceRadius = 150.f;

	/** How often to re-path toward the player's current position. */
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (Units = "s"))
	float RepathInterval = 0.5f;

	/** Internal: seconds accumulated since the last re-path. */
	UPROPERTY()
	float TimeSinceLastRepath = 0.f;
};

/**
 * StateTree task: repeatedly moves Character toward the local player pawn.
 * Succeeds once within AcceptanceRadius - this is the natural future hook
 * point for routing into combat (BeginBattle(), per AstralMageCharacter's own
 * TurnedHostile TODO), not wired here - task 3's scope.
 */
USTRUCT(meta = (DisplayName = "Astral Aggro Chase", Category = "Wildlife"))
struct FStateTreeAstralAggroChaseTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	FStateTreeAstralAggroChaseTask()
	{
		bShouldCallTick = true;
	}

	using FInstanceDataType = FStateTreeAstralAggroChaseTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
