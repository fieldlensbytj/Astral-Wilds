// Astral Wilds - the only sanctioned ways to change economy state (wallet,
// inventory, the Wayfarer Commission, and save round-tripping). Ported from
// the validated Unity prototype (AstralWallet.cs / AstralInventory.cs /
// AstralVendorService.cs / AstralWayfarerCommission.cs) so the same
// earned-only guarantees carry over: every function here checks its
// preconditions before mutating anything, so a rejected transaction never
// partially applies. See Docs/Design/EconomyPolicy.md - this is the
// non-negotiable "no real-money economy" rule's C++ implementation.
#pragma once

#include "CoreMinimal.h"
#include "AstralEconomyTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AstralEconomyRules.generated.h"

UCLASS()
class UAstralEconomyRules : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Price of one Field Tonic at the Wayfarer Supply Relay, in Starshards. */
	static constexpr int64 FieldTonicPrice = 30;

	/** Starshards earned per Salvaged Alloy sold back at the Wayfarer Supply Relay. */
	static constexpr int64 SalvagedAlloySaleValue = 15;

	/** Salvaged Alloy sales required to unlock the Wayfarer Commission's reward. */
	static constexpr int32 RequiredAlloySales = 2;

	/** Starshards awarded once, on completion, by the Wayfarer Commission. */
	static constexpr int64 WayfarerCommissionReward = 40;

	// --- Wallet ---

	/** Adds earned Starshards to Wallet. Fails (no mutation) if Amount <= 0 or it would overflow the balance. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryEarn(UPARAM(ref) FAstralWallet& Wallet, int64 Amount, EAstralCurrencySource Source);

	/** Spends Starshards from Wallet. Fails (no mutation) if Amount <= 0 or exceeds the balance. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TrySpend(UPARAM(ref) FAstralWallet& Wallet, int64 Amount);

	/** Earns UnitValue * Quantity Starshards as an item sale. Fails (no mutation) if either input is non-positive or the multiplication would overflow. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TrySellItems(UPARAM(ref) FAstralWallet& Wallet, int64 UnitValue, int32 Quantity);

	/** Restores a wallet balance from a save. Fails (no mutation) if SavedBalance is negative. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRestoreWallet(UPARAM(ref) FAstralWallet& Wallet, int64 SavedBalance);

	/** Resets Wallet to a zero balance (new-game state). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static void ResetWallet(UPARAM(ref) FAstralWallet& Wallet);

	// --- Inventory ---

	/** Returns the current count of Item in Inventory. */
	UFUNCTION(BlueprintPure, Category = "Astral|Economy")
	static int32 GetItemCount(const FAstralInventory& Inventory, EAstralItemId Item);

	/** True if adding Quantity of Item to Inventory would not overflow. Quantity must be positive. */
	UFUNCTION(BlueprintPure, Category = "Astral|Economy")
	static bool CanAddItem(const FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity);

	/** Adds Quantity of Item to Inventory. Fails (no mutation) if CanAddItem would return false. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryAddItem(UPARAM(ref) FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity);

	/** Removes Quantity of Item from Inventory. Fails (no mutation) if Quantity <= 0 or exceeds the current count. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRemoveItem(UPARAM(ref) FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity);

	/** Restores item counts from a save. Fails (no mutation) if either saved count is negative. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRestoreInventory(UPARAM(ref) FAstralInventory& Inventory, int32 SavedSalvagedAlloy, int32 SavedFieldTonics);

	/** Resets Inventory to empty (new-game state). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static void ResetInventory(UPARAM(ref) FAstralInventory& Inventory);

	// --- Wayfarer Supply Relay trades (earned-currency-only, atomic) ---

	/** Buys one Field Tonic for FieldTonicPrice. Fails with no mutation to either side if unaffordable or the inventory can't hold it. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryBuyFieldTonic(UPARAM(ref) FAstralWallet& Wallet, UPARAM(ref) FAstralInventory& Inventory);

	/** Sells one Salvaged Alloy for SalvagedAlloySaleValue. Fails with no mutation to either side if none is held or the wallet would overflow. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TrySellSalvagedAlloy(UPARAM(ref) FAstralWallet& Wallet, UPARAM(ref) FAstralInventory& Inventory);

	// --- Wayfarer Commission ---

	/** Records Quantity more Salvaged Alloy sold toward the commission. Fails (no mutation) if Quantity <= 0 or it would overflow. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRecordAlloySale(UPARAM(ref) FAstralWayfarerCommission& Commission, int32 Quantity);

	/** Sales still needed before the commission can complete; zero once RequiredAlloySales is met. */
	UFUNCTION(BlueprintPure, Category = "Astral|Economy")
	static int32 GetRemainingSales(const FAstralWayfarerCommission& Commission);

	/**
	 * Completes the commission and pays WayfarerCommissionReward Starshards, once.
	 * Fails (no mutation to either the commission or the wallet) if bUnlocked is
	 * false, it's already complete, the required sales aren't met yet, or the
	 * wallet would overflow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryCompleteCommission(UPARAM(ref) FAstralWayfarerCommission& Commission, bool bUnlocked, UPARAM(ref) FAstralWallet& Wallet);

	/** Restores commission progress from a save. Fails (no mutation) if SavedAlloySold is negative, or SavedCompleted is true without enough sales to justify it. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRestoreCommission(UPARAM(ref) FAstralWayfarerCommission& Commission, int32 SavedAlloySold, bool bSavedCompleted);

	/** Resets Commission to its not-yet-started state (new-game state). */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static void ResetCommission(UPARAM(ref) FAstralWayfarerCommission& Commission);

	// --- Save round-trip ---

	/** Captures Wallet and Inventory into a single save payload. */
	UFUNCTION(BlueprintPure, Category = "Astral|Economy")
	static FAstralEconomySaveData CaptureEconomySaveData(const FAstralWallet& Wallet, const FAstralInventory& Inventory);

	/** True if SaveData's fields are all non-negative - the only thing that makes a payload safe to restore. */
	UFUNCTION(BlueprintPure, Category = "Astral|Economy")
	static bool IsEconomySaveDataValid(const FAstralEconomySaveData& SaveData);

	/** Restores Wallet and Inventory from SaveData. Fails (no mutation to either) if IsEconomySaveDataValid would return false. */
	UFUNCTION(BlueprintCallable, Category = "Astral|Economy")
	static bool TryRestoreEconomySaveData(const FAstralEconomySaveData& SaveData, UPARAM(ref) FAstralWallet& Wallet, UPARAM(ref) FAstralInventory& Inventory);
};
