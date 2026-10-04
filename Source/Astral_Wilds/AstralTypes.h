// Astral Wilds - core gameplay enums and lightweight combat types shared across
// the Mage character, the Astral combat rules, and (eventually) the Resonance
// Weave bonding system. Kept dependency-light so it can be included anywhere.
#pragma once

#include "CoreMinimal.h"
#include "AstralTypes.generated.h"

class UAstralSpeciesData;

/**
 * The ten Astral Essences (see the Astral Wilds Canon Bible, Section III).
 * Doubles as the Astral type system: bitmask-capable (values are already
 * sequential from 0, which is what Bitflags needs) so future type-effectiveness
 * lookups can treat this as a flag set, while species data (see
 * UAstralSpeciesData) uses it as two plain scalars - Primary/Secondary - for
 * Pokemon-style dual-typing rather than an open bitmask combination.
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
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

/** Behavioral state a wild Astral is in before it can be bonded with (Canon Bible: "create receptiveness"). Shared by AAstralCharacter and the legacy AWildAstralEncounter. */
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

/** Base (level 1, unscaled) stat spread for a species. Shared by UAstralSpeciesData and FAstralCombatant. */
USTRUCT(BlueprintType)
struct FAstralBaseStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Stats")
	int32 HP = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Stats")
	int32 Attack = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Stats")
	int32 Defense = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Stats")
	int32 Speed = 10;
};

/**
 * Lightweight HP/identity record for a party member, reserve member, or
 * opponent slot. Mirrors AstralDemoMember from the Unity prototype so the
 * same battle math can be validated the same way on both engines. Extended
 * beyond the original Hp/MaxHp fields with species/level/XP/stats so a single
 * FAstralCombatant can double as a roster entry - see AAstralCharacter's
 * ToCombatant()/InitFromCombatant() for the conversion boundary.
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
	bool bHasSecondaryEssence = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	EAstralEssence SecondaryEssence = EAstralEssence::Ember;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 Hp = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 MaxHp = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	bool bDefeated = false;

	/** The species this combatant belongs to, if any (roster/wild entries populate this; hand-authored opponents may leave it null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	TObjectPtr<UAstralSpeciesData> SpeciesData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	int32 CurrentXP = 0;

	/** SpeciesData->BaseStats scaled by Level at the time this snapshot was taken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral")
	FAstralBaseStats CurrentStats;
};
