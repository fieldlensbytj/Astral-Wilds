// Astral Wilds - AI controller for roaming wild Astrals. Mirrors
// Variant_Combat/AI/CombatAIController.h exactly (owns a UStateTreeAIComponent,
// defers StartLogic() until OnPossess, bAttachToPawn=true for EQS/queries to
// work correctly). Not abstract, unlike ACombatAIController - there's no
// per-archetype controller subclassing here, the one shared StateTree graph
// (see AstralWildlifeStateTreeUtility.h) branches on SpeciesData->AIArchetype
// itself via FStateTreeAstralArchetypeCondition.
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AstralWildlifeController.generated.h"

class UStateTreeAIComponent;

UCLASS()
class AAstralWildlifeController : public AAIController
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStateTreeAIComponent* StateTreeAI;

public:

	AAstralWildlifeController();

protected:

	virtual void OnPossess(APawn* InPawn) override;
};
