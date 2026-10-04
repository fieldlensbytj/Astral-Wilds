#include "AstralWildEncounter.h"
#include "Components/SphereComponent.h"

AWildAstralEncounter::AWildAstralEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
	InteractSphere->InitSphereRadius(InteractRadius);
	InteractSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = InteractSphere;
}

void AWildAstralEncounter::TryBecomeReceptive()
{
	if (WildState == EAstralWildState::Enraged || WildState == EAstralWildState::Territorial)
	{
		// Aggressive species don't simply calm down on their own - the Canon
		// Bible calls for combat/exhaustion first. Leaving this as a no-op
		// here (rather than a silent transition) means a Blueprint subclass
		// MUST implement real behavior for these states before they can ever
		// become bondable, instead of quietly falling back to "always works."
		return;
	}

	WildState = EAstralWildState::Receptive;
}
