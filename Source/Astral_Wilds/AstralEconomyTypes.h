// Astral Wilds - earned-only economy data types. Ported from the validated
// Unity prototype (AstralWallet.cs / AstralInventory.cs / AstralWayfarerCommission.cs)
// per Docs/Design/EconomyPolicy.md's non-negotiable rule: every currency,
// item, and quest reward in this game is earned through gameplay alone, never
// purchased. These structs deliberately have no store, payment, entitlement,
// or advertising concepts anywhere - see AstralEconomyRules.h for the logic
// that operates on them.
#pragma once

#include "CoreMinimal.h"
#include "AstralEconomyTypes.generated.h"

/** Where a Starshard reward came from. Mirrors AstralCurrencySource from the Unity prototype. */
UENUM(BlueprintType)
enum class EAstralCurrencySource : uint8
{
	BossVictory		UMETA(DisplayName = "Boss Victory"),
	EncounterClear		UMETA(DisplayName = "Encounter Clear"),
	ExplorationFind		UMETA(DisplayName = "Exploration Find"),
	ItemSale		UMETA(DisplayName = "Item Sale"),
	QuestReward		UMETA(DisplayName = "Quest Reward")
};

/** The field-economy items tracked by FAstralInventory. Mirrors AstralItemId from the Unity prototype. */
UENUM(BlueprintType)
enum class EAstralItemId : uint8
{
	SalvagedAlloy		UMETA(DisplayName = "Salvaged Alloy"),
	FieldTonic		UMETA(DisplayName = "Field Tonic")
};

/**
 * Earned-gameplay currency balance (Starshards). Deliberately holds nothing
 * but a non-negative balance - no store, payment, entitlement, advertising,
 * or premium-currency concepts. See UAstralEconomyRules for the only
 * sanctioned ways to change Balance.
 */
USTRUCT(BlueprintType)
struct FAstralWallet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astral|Economy")
	int64 Balance = 0;
};

/**
 * Small deterministic item inventory used by the field economy. Counts are
 * plain non-negative values so transactions and save validation stay
 * auditable, matching the Unity prototype's AstralInventory.
 */
USTRUCT(BlueprintType)
struct FAstralInventory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astral|Economy")
	int32 SalvagedAlloy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astral|Economy")
	int32 FieldTonics = 0;
};

/**
 * The Wayfarer Commission: a small post-expedition quest fueled entirely by
 * gameplay-earned salvage (Docs/Design/EconomyPolicy.md). Mirrors
 * AstralWayfarerCommission from the Unity prototype.
 */
USTRUCT(BlueprintType)
struct FAstralWayfarerCommission
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astral|Economy")
	int32 AlloySold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astral|Economy")
	bool bCompleted = false;
};

/** Versioned-save payload for the earned-only economy. Mirrors AstralEconomySaveData from the Unity prototype. */
USTRUCT(BlueprintType)
struct FAstralEconomySaveData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Economy")
	int64 Starshards = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Economy")
	int32 SalvagedAlloy = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Astral|Economy")
	int32 FieldTonics = 0;
};
