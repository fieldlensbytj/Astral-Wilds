// Astral Wilds - core gameplay enums and lightweight combat types shared across
// the Mage character, the Astral combat rules, and (eventually) the Resonance
// Weave bonding system. Kept dependency-light so it can be included anywhere.
#pragma once

#include "CoreMinimal.h"
#include "AstralTypes.generated.h"

/** The ten Astral Essences (see the Astral Wilds Canon Bible, Section III). */
UENUM(BlueprintType)
enum class EAstralEssence : uint8
{
	Ember		UMETA(DisplayName = "Ember"),
	Verdant		UMETA(DisplayName = "Verdant"),
	Terra		UMETA(DisplayName = "Terra"),
	Tide		UMETA(DisplayName = "Tide"),
	Frost		UMETA(DisplayName = "Frost"),
	Volt		UMETA(DisplayName = "Volt"),
	Gale		UMETA(DisplayName = "Gale"),
	Radiant		UMETA(DisplayName = "Radiant"),
	Umbral		UMETA(DisplayName = "Umbral"),
	Arcane		UMETA(DisplayName = "Arcane")
};

/** Result of attempting a combat action (attack, Arc Burst, guard). */
UENUM(BlueprintType)
enum class EAstralActionOutcome : uint8
{
	Invalid			UMETA(DisplayName = "Invalid"),
	SlotAlreadyActed	UMETA(DisplayName = "Slot Already Acted"),
	Applied			UMETA(DisplayName = "Applied")
};

/** Which side of the Covenant of Two a combatant belongs to. */
UENUM(BlueprintType)
enum class EAstralBattleSide : uint8
{
	Player		UMETA(DisplayName = "Player"),
	Opponent	UMETA(DisplayName = "Opponent")
};

/** Warden legitimacy classification (Canon Bible, Section IX) — Warden predates both the Houses and the Dominion. */
UENUM(BlueprintType)
enum class EAstralWardenType : uint8
{
	CrownWarden	UMETA(DisplayName = "Crown Warden"),
	FreeWarden	UMETA(DisplayName = "Free Warden"),
	ExiledWarden	UMETA(DisplayName = "Exiled Warden")
};

/** Wand Frame combat identity (Canon Bible, Section VII) — four archetypes; Core/Focus/Sigil flavor how each plays. */
UENUM(BlueprintType)
enum class EAstralMageFrame : uint8
{
	Invoker		UMETA(DisplayName = "Invoker (ranged spellcasting)"),
	Duelist		UMETA(DisplayName = "Duelist (close-range Aether weapon)"),
	Bulwark		UMETA(DisplayName = "Bulwark (guard, counter, barrier)"),
	Conductor	UMETA(DisplayName = "Conductor (support, buff/debuff, control)")
};

/** Outcome of a single Resonance Weave attempt. */
UENUM(BlueprintType)
enum class EAstralWeaveResult : uint8
{
	InProgress	UMETA(DisplayName = "In Progress"),
	Succeeded	UMETA(DisplayName = "Succeeded"),
	Fled		UMETA(DisplayName = "Astral Fled"),
	TurnedHostile	UMETA(DisplayName = "Astral Turned Hostile"),
	MayRetry	UMETA(DisplayName = "May Retry")
};

/**
 * Lightweight HP/identity record for a party member, reserve member, or
 * opponent slot. Mirrors AstralDemoMember from the Unity prototype so the
 * same battle math can be validated the same way on both engines.
 */
USTRUCT(BlueprintType)
struct FAstralCombatant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	FString Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	EAstralEssence PrimaryEssence = EAstralEssence::Ember;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 Hp = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 MaxHp = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	bool bDefeated = false;
};
