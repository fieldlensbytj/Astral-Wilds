// Astral Wilds - see AstralWildlifeController.h.
#include "AstralWildlifeController.h"
#include "Components/StateTreeAIComponent.h"

AAstralWildlifeController::AAstralWildlifeController()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	// Ensure we start the StateTree ourselves once the pawn is fully possessed.
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);

	// Necessary for EnvQueries/navigation queries to work correctly.
	bAttachToPawn = true;
}

void AAstralWildlifeController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	StateTreeAI->StartLogic();
}
