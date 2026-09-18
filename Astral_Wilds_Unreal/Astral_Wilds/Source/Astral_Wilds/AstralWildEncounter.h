// Astral Wilds - a wild Astral standing in the world, ready to be observed,
// made receptive (Canon Bible Section VII, Step 1), and bonded with via the
// Resonance Weave. This is the bridge between exploration and the Wand/
// AstralMageCharacter systems: DoInteract() traces for the nearest one of
// these, reads its Temperament and State, and starts a real weave attempt
// instead of the placeholder default that shipped earlier.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AstralTypes.h"
#include "AstralResonanceWeaveComponent.h"
#include "AstralWildEncounter.generated.h"

class USphereComponent;

/** Behavioral state a wild Astral is in before it can be bonded with (Canon Bible: "create receptiveness"). */
UENUM(BlueprintType)
enum class EAstralWildState : uint8
{
	Calm		UMETA(DisplayName = "Calm"),
	Curious		UMETA(DisplayName = "Curious"),
	Wary		UMETA(DisplayName = "Wary"),
	Territorial	UMETA(DisplayName = "Territorial"),
	Frightened	UMETA(DisplayName = "Frightened"),
	Enraged		UMETA(DisplayName = "Enraged"),
	Receptive	UMETA(DisplayName = "Receptive")
};

/**
 * A wild Astral encounter placed in the world. Level designers (or a future
 * spawner) set Combatant/Temperament/State in the editor or via SetWildState;
 * the Mage traces for these and, once Receptive, can attempt a Resonance
 * Weave against them.
 */
UCLASS()
class AWildAstralEncounter : public AActor
{
	GENERATED_BODY()

public:

	AWildAstralEncounter();

	/** Identity and stats if this Astral joins the player's party or is fought as an opponent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	FAstralCombatant Combatant;

	/** How difficult/eventful bonding with this Astral is, per its species (Canon Bible Section VII). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Bonding")
	FAstralWeaveTemperament Temperament;

	/** Radius within which the Mage's interact trace can detect this Astral. */
	UPROPERTY(EditAnywhere, Category = "Astral")
	float InteractRadius = 250.f;

	UFUNCTION(BlueprintPure, Category = "Astral")
	EAstralWildState GetWildState() const { return WildState; }

	UFUNCTION(BlueprintPure, Category = "Astral")
	bool IsReceptive() const { return WildState == EAstralWildState::Receptive; }

	/**
	 * Moves this Astral toward Receptive, per its own species behavior.
	 * Base implementation is a simple direct transition - override in
	 * Blueprint (or a C++ subclass) for real per-species behavior: exhaust an
	 * aggressive predator through combat, offer food to a timid one, heal an
	 * injured one, complete a task for an intelligent one, and so on, per the
	 * Canon Bible.
	 */
	UFUNCTION(BlueprintCallable, Category = "Astral")
	virtual void TryBecomeReceptive();

	UFUNCTION(BlueprintCallable, Category = "Astral")
	void SetWildState(EAstralWildState NewState) { WildState = NewState; }

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* InteractSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	EAstralWildState WildState = EAstralWildState::Calm;
};
