// Astral Wilds - automation tests for AAstralMageCharacter that need a real
// possessed pawn. Each test builds a throwaway UWorld, spawns the Mage and a
// PlayerController, and drives possession by hand; the world never ticks.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralMageCharacter.h"
#include "AstralCharacter.h"
#include "AstralSpeciesData.h"
#include "AstralResonanceWeaveComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"

// Repro for the Ensure first seen 2026-09-18 ("InvocationList[ CurFunctionIndex ]
// != InDelegate" from SetupPlayerInputComponent's OnWeaveResult.AddDynamic):
// APawn::PawnClientRestart rebuilds the input component and re-runs
// SetupPlayerInputComponent on every possess, so re-possessing the same Mage
// must not bind the weave-result handler a second time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralMage_RepossessBindsWeaveResultOnce, "AstralWilds.Mage.RepossessBindsWeaveResultOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralMage_RepossessBindsWeaveResultOnce::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AstralMageTestWorld"));
	// ClientRestart is an RPC; AActor::ProcessEvent drops it until actors are initialized.
	World->InitializeActorsForPlay(FURL());
	AAstralMageCharacter* Mage = World->SpawnActor<AAstralMageCharacter>();
	APlayerController* PC = World->SpawnActor<APlayerController>();
	UAstralResonanceWeaveComponent* Weave = Mage ? Mage->FindComponentByClass<UAstralResonanceWeaveComponent>() : nullptr;

	if (TestNotNull(TEXT("Mage spawned"), Mage) && TestNotNull(TEXT("PlayerController spawned"), PC) && TestNotNull(TEXT("Mage has a ResonanceWeave"), Weave))
	{
		// With no NetDriver, a PlayerController only counts as local (and so
		// only builds pawn input) when it owns a ULocalPlayer.
		PC->Player = NewObject<ULocalPlayer>(GEngine);

		PC->Possess(Mage);
		TestNotNull(TEXT("First possess built an input component"), Mage->InputComponent.Get());
		TestEqual(TEXT("Weave result bound once after first possess"), Weave->OnWeaveResult.GetAllObjects().Num(), 1);

		PC->UnPossess();
		PC->Possess(Mage);
		TestNotNull(TEXT("Re-possess rebuilt the input component"), Mage->InputComponent.Get());
		TestEqual(TEXT("Weave result still bound once after re-possess"), Weave->OnWeaveResult.GetAllObjects().Num(), 1);
	}

	World->DestroyWorld(false);
	return true;
}

// End-to-end bonding loop through the real Mage code paths: a receptive wild
// Astral in front of the Mage, Interact (overlap query against its
// InteractSphere) begins a weave with the species' temperament, the player
// tracks + channels + answers each pulse, and on success the Mage's
// OnResonanceWeaveResult adds it to the party and removes it from the world.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralMage_BondingLoopAddsAstralToParty, "AstralWilds.Mage.BondingLoopAddsAstralToParty", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralMage_BondingLoopAddsAstralToParty::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("AstralBondingTestWorld"));
	World->InitializeActorsForPlay(FURL());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAstralMageCharacter* Mage = World->SpawnActor<AAstralMageCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	AAstralCharacter* Wild = World->SpawnActor<AAstralCharacter>(FVector(300.f, 0.f, 0.f), FRotator::ZeroRotator, Params);
	APlayerController* PC = World->SpawnActor<APlayerController>();

	// Galevine's capture rate; other temperament fields at their defaults.
	UAstralSpeciesData* Species = NewObject<UAstralSpeciesData>();
	Species->SpeciesName = FText::FromString(TEXT("Galevine"));
	Species->CaptureRate = 90;

	if (TestNotNull(TEXT("Mage spawned"), Mage) && TestNotNull(TEXT("Wild Astral spawned"), Wild) && TestNotNull(TEXT("PlayerController spawned"), PC))
	{
		Wild->SpeciesData = Species;
		Wild->WildState = EAstralWildState::Receptive;
		PC->Player = NewObject<ULocalPlayer>(GEngine);
		PC->Possess(Mage);

		TestTrue(TEXT("Receptive Astral in reach is offered for Interact"), Mage->GetInteractableWildAstral() == Wild);

		Mage->DoInteract();
		UAstralResonanceWeaveComponent* Weave = Mage->GetResonanceWeave();
		TestTrue(TEXT("Interact began a weave"), Mage->IsWeavingResonance());
		TestTrue(TEXT("Weave targets the Astral"), Mage->GetCurrentWeaveTarget() == Wild);

		// Play it well: track the point, hold Channel, answer every pulse.
		Weave->SetChanneling(true);
		float Elapsed = 0.f;
		while (Weave->IsWeaveActive() && Elapsed < 30.f)
		{
			Weave->SetAlignmentInput(Weave->GetResonancePoint() - Weave->GetAlignmentReticle());
			if (Weave->IsAwaitingPulseResponse())
			{
				Weave->RespondToHarmonize();
			}
			static_cast<UActorComponent*>(Weave)->TickComponent(0.01f, LEVELTICK_All, nullptr);
			Elapsed += 0.01f;
		}

		TestFalse(TEXT("Weave resolved"), Mage->IsWeavingResonance());
		TestTrue(TEXT("Skilled play bonds a Galevine in under 10s"), Elapsed < 10.f);
		if (TestEqual(TEXT("Astral joined the party"), Mage->GetParty().Num(), 1))
		{
			TestEqual(TEXT("Party member keeps its species"), Mage->GetParty()[0].SpeciesData.Get(), Species);
		}
		TestFalse(TEXT("Bonded Astral left the world"), IsValid(Wild));
		TestNull(TEXT("Weave target cleared"), Mage->GetCurrentWeaveTarget());
	}

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
