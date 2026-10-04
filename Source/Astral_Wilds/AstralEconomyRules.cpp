#include "AstralEconomyRules.h"

// --- Wallet ---

bool UAstralEconomyRules::TryEarn(FAstralWallet& Wallet, int64 Amount, EAstralCurrencySource Source)
{
	(void)Source; // Reserved for future analytics/telemetry; not needed to validate the transaction itself.

	if (Amount <= 0 || Wallet.Balance > TNumericLimits<int64>::Max() - Amount)
	{
		return false;
	}

	Wallet.Balance += Amount;
	return true;
}

bool UAstralEconomyRules::TrySpend(FAstralWallet& Wallet, int64 Amount)
{
	if (Amount <= 0 || Amount > Wallet.Balance)
	{
		return false;
	}

	Wallet.Balance -= Amount;
	return true;
}

bool UAstralEconomyRules::TrySellItems(FAstralWallet& Wallet, int64 UnitValue, int32 Quantity)
{
	if (UnitValue <= 0 || Quantity <= 0 || UnitValue > TNumericLimits<int64>::Max() / Quantity)
	{
		return false;
	}

	return TryEarn(Wallet, UnitValue * Quantity, EAstralCurrencySource::ItemSale);
}

bool UAstralEconomyRules::TryRestoreWallet(FAstralWallet& Wallet, int64 SavedBalance)
{
	if (SavedBalance < 0)
	{
		return false;
	}

	Wallet.Balance = SavedBalance;
	return true;
}

void UAstralEconomyRules::ResetWallet(FAstralWallet& Wallet)
{
	Wallet.Balance = 0;
}

// --- Inventory ---

int32 UAstralEconomyRules::GetItemCount(const FAstralInventory& Inventory, EAstralItemId Item)
{
	switch (Item)
	{
	case EAstralItemId::SalvagedAlloy: return Inventory.SalvagedAlloy;
	case EAstralItemId::FieldTonic: return Inventory.FieldTonics;
	default: return 0;
	}
}

bool UAstralEconomyRules::CanAddItem(const FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity)
{
	if (Quantity <= 0)
	{
		return false;
	}

	return GetItemCount(Inventory, Item) <= TNumericLimits<int32>::Max() - Quantity;
}

bool UAstralEconomyRules::TryAddItem(FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity)
{
	if (!CanAddItem(Inventory, Item, Quantity))
	{
		return false;
	}

	const int32 NewCount = GetItemCount(Inventory, Item) + Quantity;
	switch (Item)
	{
	case EAstralItemId::SalvagedAlloy: Inventory.SalvagedAlloy = NewCount; break;
	case EAstralItemId::FieldTonic: Inventory.FieldTonics = NewCount; break;
	default: return false;
	}
	return true;
}

bool UAstralEconomyRules::TryRemoveItem(FAstralInventory& Inventory, EAstralItemId Item, int32 Quantity)
{
	if (Quantity <= 0 || GetItemCount(Inventory, Item) < Quantity)
	{
		return false;
	}

	const int32 NewCount = GetItemCount(Inventory, Item) - Quantity;
	switch (Item)
	{
	case EAstralItemId::SalvagedAlloy: Inventory.SalvagedAlloy = NewCount; break;
	case EAstralItemId::FieldTonic: Inventory.FieldTonics = NewCount; break;
	default: return false;
	}
	return true;
}

bool UAstralEconomyRules::TryRestoreInventory(FAstralInventory& Inventory, int32 SavedSalvagedAlloy, int32 SavedFieldTonics)
{
	if (SavedSalvagedAlloy < 0 || SavedFieldTonics < 0)
	{
		return false;
	}

	Inventory.SalvagedAlloy = SavedSalvagedAlloy;
	Inventory.FieldTonics = SavedFieldTonics;
	return true;
}

void UAstralEconomyRules::ResetInventory(FAstralInventory& Inventory)
{
	Inventory.SalvagedAlloy = 0;
	Inventory.FieldTonics = 0;
}

// --- Wayfarer Supply Relay trades ---

bool UAstralEconomyRules::TryBuyFieldTonic(FAstralWallet& Wallet, FAstralInventory& Inventory)
{
	if (Wallet.Balance < FieldTonicPrice || !CanAddItem(Inventory, EAstralItemId::FieldTonic, 1))
	{
		return false;
	}

	// Both operations are now guaranteed by the checked preconditions.
	return TrySpend(Wallet, FieldTonicPrice) && TryAddItem(Inventory, EAstralItemId::FieldTonic, 1);
}

bool UAstralEconomyRules::TrySellSalvagedAlloy(FAstralWallet& Wallet, FAstralInventory& Inventory)
{
	if (GetItemCount(Inventory, EAstralItemId::SalvagedAlloy) < 1 ||
		Wallet.Balance > TNumericLimits<int64>::Max() - SalvagedAlloySaleValue)
	{
		return false;
	}

	// Both operations are now guaranteed by the checked preconditions.
	return TryRemoveItem(Inventory, EAstralItemId::SalvagedAlloy, 1) &&
		TryEarn(Wallet, SalvagedAlloySaleValue, EAstralCurrencySource::ItemSale);
}

// --- Wayfarer Commission ---

bool UAstralEconomyRules::TryRecordAlloySale(FAstralWayfarerCommission& Commission, int32 Quantity)
{
	if (Quantity <= 0 || Commission.AlloySold > TNumericLimits<int32>::Max() - Quantity)
	{
		return false;
	}

	Commission.AlloySold += Quantity;
	return true;
}

int32 UAstralEconomyRules::GetRemainingSales(const FAstralWayfarerCommission& Commission)
{
	return FMath::Max(0, RequiredAlloySales - Commission.AlloySold);
}

bool UAstralEconomyRules::TryCompleteCommission(FAstralWayfarerCommission& Commission, bool bUnlocked, FAstralWallet& Wallet)
{
	if (!bUnlocked || Commission.bCompleted || Commission.AlloySold < RequiredAlloySales ||
		!TryEarn(Wallet, WayfarerCommissionReward, EAstralCurrencySource::QuestReward))
	{
		return false;
	}

	Commission.bCompleted = true;
	return true;
}

bool UAstralEconomyRules::TryRestoreCommission(FAstralWayfarerCommission& Commission, int32 SavedAlloySold, bool bSavedCompleted)
{
	if (SavedAlloySold < 0 || (bSavedCompleted && SavedAlloySold < RequiredAlloySales))
	{
		return false;
	}

	Commission.AlloySold = SavedAlloySold;
	Commission.bCompleted = bSavedCompleted;
	return true;
}

void UAstralEconomyRules::ResetCommission(FAstralWayfarerCommission& Commission)
{
	Commission.AlloySold = 0;
	Commission.bCompleted = false;
}

// --- Save round-trip ---

FAstralEconomySaveData UAstralEconomyRules::CaptureEconomySaveData(const FAstralWallet& Wallet, const FAstralInventory& Inventory)
{
	FAstralEconomySaveData SaveData;
	SaveData.Starshards = Wallet.Balance;
	SaveData.SalvagedAlloy = Inventory.SalvagedAlloy;
	SaveData.FieldTonics = Inventory.FieldTonics;
	return SaveData;
}

bool UAstralEconomyRules::IsEconomySaveDataValid(const FAstralEconomySaveData& SaveData)
{
	return SaveData.Starshards >= 0 && SaveData.SalvagedAlloy >= 0 && SaveData.FieldTonics >= 0;
}

bool UAstralEconomyRules::TryRestoreEconomySaveData(const FAstralEconomySaveData& SaveData, FAstralWallet& Wallet, FAstralInventory& Inventory)
{
	if (!IsEconomySaveDataValid(SaveData))
	{
		return false;
	}

	// Validation above makes both restores guaranteed, which is what keeps this atomic.
	return TryRestoreWallet(Wallet, SaveData.Starshards) &&
		TryRestoreInventory(Inventory, SaveData.SalvagedAlloy, SaveData.FieldTonics);
}
