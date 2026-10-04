// Astral Wilds - automation tests for the earned-only economy (wallet,
// inventory, Wayfarer Supply Relay trades, the Wayfarer Commission, and save
// round-tripping). Pure logic tests: no world, no actors, no Play-in-Editor
// required - same discipline as AstralCombatRulesTests.cpp. Ported from the
// validated Unity EditMode suite (Assets/Tests/EditMode/AstralWalletTests.cs,
// AstralInventoryVendorTests.cs, AstralWayfarerCommissionTests.cs); every
// case here has a matching case there, so the earned-only guarantees in
// Docs/Design/EconomyPolicy.md stay enforced on both engines.
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AstralEconomyRules.h"

// --- Wallet ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_GameplayRewardsIncreaseBalance, "AstralWilds.Economy.Wallet.GameplayRewardsIncreaseBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_GameplayRewardsIncreaseBalance::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	TestTrue(TEXT("Boss victory reward is earned"), UAstralEconomyRules::TryEarn(Wallet, 50, EAstralCurrencySource::BossVictory));
	TestTrue(TEXT("Exploration find reward is earned"), UAstralEconomyRules::TryEarn(Wallet, 25, EAstralCurrencySource::ExplorationFind));
	TestEqual(TEXT("Balance accumulates both rewards"), Wallet.Balance, (int64)75);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_SpendRequiresPositiveAffordableAmount, "AstralWilds.Economy.Wallet.SpendRequiresPositiveAffordableAmount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_SpendRequiresPositiveAffordableAmount::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	UAstralEconomyRules::TryEarn(Wallet, 40, EAstralCurrencySource::QuestReward);
	TestFalse(TEXT("Spending zero fails"), UAstralEconomyRules::TrySpend(Wallet, 0));
	TestFalse(TEXT("Spending more than the balance fails"), UAstralEconomyRules::TrySpend(Wallet, 41));
	TestTrue(TEXT("Spending an affordable amount succeeds"), UAstralEconomyRules::TrySpend(Wallet, 15));
	TestEqual(TEXT("Balance reflects the successful spend only"), Wallet.Balance, (int64)25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_ItemSalesUseOnlyPositiveValueAndQuantity, "AstralWilds.Economy.Wallet.ItemSalesUseOnlyPositiveValueAndQuantity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_ItemSalesUseOnlyPositiveValueAndQuantity::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	TestFalse(TEXT("Zero unit value is rejected"), UAstralEconomyRules::TrySellItems(Wallet, 0, 3));
	TestFalse(TEXT("Zero quantity is rejected"), UAstralEconomyRules::TrySellItems(Wallet, 10, 0));
	TestTrue(TEXT("Positive value and quantity succeed"), UAstralEconomyRules::TrySellItems(Wallet, 12, 3));
	TestEqual(TEXT("Balance is unit value times quantity"), Wallet.Balance, (int64)36);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_RestoreRejectsNegativeBalance, "AstralWilds.Economy.Wallet.RestoreRejectsNegativeBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_RestoreRejectsNegativeBalance::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	TestFalse(TEXT("A negative saved balance is rejected"), UAstralEconomyRules::TryRestoreWallet(Wallet, -1));
	TestEqual(TEXT("Balance is untouched by the rejected restore"), Wallet.Balance, (int64)0);
	TestTrue(TEXT("A valid saved balance restores"), UAstralEconomyRules::TryRestoreWallet(Wallet, 125));
	TestEqual(TEXT("Balance reflects the restored value"), Wallet.Balance, (int64)125);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_OverflowingRewardIsRejectedWithoutChangingBalance, "AstralWilds.Economy.Wallet.OverflowingRewardIsRejectedWithoutChangingBalance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_OverflowingRewardIsRejectedWithoutChangingBalance::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	UAstralEconomyRules::TryRestoreWallet(Wallet, TNumericLimits<int64>::Max() - 5);
	TestFalse(TEXT("A reward that would overflow the balance is rejected"), UAstralEconomyRules::TryEarn(Wallet, 6, EAstralCurrencySource::BossVictory));
	TestEqual(TEXT("Balance is unchanged by the rejected overflow"), Wallet.Balance, TNumericLimits<int64>::Max() - 5);
	return true;
}

// --- Inventory and Wayfarer Supply Relay trades ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_Inventory_AddRemoveAndRestore_AreValidated, "AstralWilds.Economy.Inventory.AddRemoveAndRestoreAreValidated", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_Inventory_AddRemoveAndRestore_AreValidated::RunTest(const FString& Parameters)
{
	FAstralInventory Inventory;

	TestTrue(TEXT("Adding alloy succeeds"), UAstralEconomyRules::TryAddItem(Inventory, EAstralItemId::SalvagedAlloy, 2));
	TestTrue(TEXT("Removing alloy succeeds"), UAstralEconomyRules::TryRemoveItem(Inventory, EAstralItemId::SalvagedAlloy, 1));
	TestEqual(TEXT("Alloy count reflects add then remove"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 1);
	TestTrue(TEXT("A valid restore succeeds"), UAstralEconomyRules::TryRestoreInventory(Inventory, 4, 3));
	TestEqual(TEXT("Restored alloy count"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 4);
	TestEqual(TEXT("Restored tonic count"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::FieldTonic), 3);
	TestFalse(TEXT("A negative saved count is rejected"), UAstralEconomyRules::TryRestoreInventory(Inventory, -1, 0));
	TestEqual(TEXT("Alloy count is untouched by the rejected restore"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_BuyFieldTonic_SucceedsAtomically, "AstralWilds.Economy.Vendor.BuyFieldTonicSucceedsAtomically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_BuyFieldTonic_SucceedsAtomically::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;
	UAstralEconomyRules::TryEarn(Wallet, 45, EAstralCurrencySource::EncounterClear);

	TestTrue(TEXT("Buying a Field Tonic succeeds when affordable"), UAstralEconomyRules::TryBuyFieldTonic(Wallet, Inventory));
	TestEqual(TEXT("Balance is reduced by the Field Tonic price"), Wallet.Balance, (int64)15);
	TestEqual(TEXT("Inventory gains one Field Tonic"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::FieldTonic), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_BuyFieldTonic_WhenUnaffordable_DoesNotMutateEitherSide, "AstralWilds.Economy.Vendor.BuyFieldTonicWhenUnaffordableDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_BuyFieldTonic_WhenUnaffordable_DoesNotMutateEitherSide::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;
	UAstralEconomyRules::TryEarn(Wallet, 29, EAstralCurrencySource::ExplorationFind);

	TestFalse(TEXT("Buying a Field Tonic fails when one Starshard short"), UAstralEconomyRules::TryBuyFieldTonic(Wallet, Inventory));
	TestEqual(TEXT("Balance is unchanged by the failed purchase"), Wallet.Balance, (int64)29);
	TestEqual(TEXT("Inventory is unchanged by the failed purchase"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::FieldTonic), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_SellSalvagedAlloy_SucceedsAtomically, "AstralWilds.Economy.Vendor.SellSalvagedAlloySucceedsAtomically", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_SellSalvagedAlloy_SucceedsAtomically::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;
	UAstralEconomyRules::TryAddItem(Inventory, EAstralItemId::SalvagedAlloy, 2);

	TestTrue(TEXT("Selling alloy succeeds when held"), UAstralEconomyRules::TrySellSalvagedAlloy(Wallet, Inventory));
	TestEqual(TEXT("Balance reflects the sale value"), Wallet.Balance, UAstralEconomyRules::SalvagedAlloySaleValue);
	TestEqual(TEXT("Inventory loses exactly one alloy"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_SellSalvagedAlloy_WhenMissingItem_DoesNotMutateEitherSide, "AstralWilds.Economy.Vendor.SellSalvagedAlloyWhenMissingItemDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_SellSalvagedAlloy_WhenMissingItem_DoesNotMutateEitherSide::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;

	TestFalse(TEXT("Selling alloy fails with none held"), UAstralEconomyRules::TrySellSalvagedAlloy(Wallet, Inventory));
	TestEqual(TEXT("Balance is unchanged"), Wallet.Balance, (int64)0);
	TestEqual(TEXT("Inventory is unchanged"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_SellSalvagedAlloy_WhenWalletWouldOverflow_DoesNotMutateEitherSide, "AstralWilds.Economy.Vendor.SellSalvagedAlloyWhenWalletWouldOverflowDoesNotMutate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_SellSalvagedAlloy_WhenWalletWouldOverflow_DoesNotMutateEitherSide::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;
	UAstralEconomyRules::TryRestoreWallet(Wallet, TNumericLimits<int64>::Max() - UAstralEconomyRules::SalvagedAlloySaleValue + 1);
	UAstralEconomyRules::TryAddItem(Inventory, EAstralItemId::SalvagedAlloy, 1);

	TestFalse(TEXT("Selling alloy fails when it would overflow the wallet"), UAstralEconomyRules::TrySellSalvagedAlloy(Wallet, Inventory));
	TestEqual(TEXT("Balance is unchanged"), Wallet.Balance, TNumericLimits<int64>::Max() - UAstralEconomyRules::SalvagedAlloySaleValue + 1);
	TestEqual(TEXT("Inventory is unchanged"), UAstralEconomyRules::GetItemCount(Inventory, EAstralItemId::SalvagedAlloy), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_EconomySaveData_RoundTrip_RestoresCurrencyAndInventory, "AstralWilds.Economy.SaveData.RoundTripRestoresCurrencyAndInventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_EconomySaveData_RoundTrip_RestoresCurrencyAndInventory::RunTest(const FString& Parameters)
{
	FAstralWallet Wallet;
	FAstralInventory Inventory;
	UAstralEconomyRules::TryEarn(Wallet, 125, EAstralCurrencySource::BossVictory);
	UAstralEconomyRules::TryRestoreInventory(Inventory, 3, 2);

	const FAstralEconomySaveData SaveData = UAstralEconomyRules::CaptureEconomySaveData(Wallet, Inventory);

	FAstralWallet RestoredWallet;
	FAstralInventory RestoredInventory;
	TestTrue(TEXT("A captured save payload restores successfully"), UAstralEconomyRules::TryRestoreEconomySaveData(SaveData, RestoredWallet, RestoredInventory));
	TestEqual(TEXT("Restored balance matches the original"), RestoredWallet.Balance, (int64)125);
	TestEqual(TEXT("Restored alloy count matches the original"), UAstralEconomyRules::GetItemCount(RestoredInventory, EAstralItemId::SalvagedAlloy), 3);
	TestEqual(TEXT("Restored tonic count matches the original"), UAstralEconomyRules::GetItemCount(RestoredInventory, EAstralItemId::FieldTonic), 2);
	return true;
}

// --- Wayfarer Commission ---

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_TwoSales_AwardQuestRewardOnlyAfterUnlock, "AstralWilds.Economy.WayfarerCommission.TwoSalesAwardRewardOnlyAfterUnlock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_TwoSales_AwardQuestRewardOnlyAfterUnlock::RunTest(const FString& Parameters)
{
	FAstralWayfarerCommission Commission;
	FAstralWallet Wallet;
	UAstralEconomyRules::TryRecordAlloySale(Commission, 2);

	TestFalse(TEXT("Completion fails while locked, even with sales met"), UAstralEconomyRules::TryCompleteCommission(Commission, false, Wallet));
	TestEqual(TEXT("No reward is paid while locked"), Wallet.Balance, (int64)0);
	TestTrue(TEXT("Completion succeeds once unlocked"), UAstralEconomyRules::TryCompleteCommission(Commission, true, Wallet));
	TestEqual(TEXT("Reward is paid exactly once"), Wallet.Balance, UAstralEconomyRules::WayfarerCommissionReward);
	TestTrue(TEXT("Commission is marked complete"), Commission.bCompleted);
	TestFalse(TEXT("A second completion attempt fails"), UAstralEconomyRules::TryCompleteCommission(Commission, true, Wallet));
	TestEqual(TEXT("The reward is not paid twice"), Wallet.Balance, UAstralEconomyRules::WayfarerCommissionReward);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_IncompleteSales_DoNotAwardReward, "AstralWilds.Economy.WayfarerCommission.IncompleteSalesDoNotAwardReward", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_IncompleteSales_DoNotAwardReward::RunTest(const FString& Parameters)
{
	FAstralWayfarerCommission Commission;
	FAstralWallet Wallet;
	UAstralEconomyRules::TryRecordAlloySale(Commission, 1);

	TestFalse(TEXT("Completion fails with sales short of the requirement"), UAstralEconomyRules::TryCompleteCommission(Commission, true, Wallet));
	TestEqual(TEXT("One sale remains"), UAstralEconomyRules::GetRemainingSales(Commission), 1);
	TestEqual(TEXT("No reward is paid"), Wallet.Balance, (int64)0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_WalletOverflow_PreventsCompletionWithoutLosingProgress, "AstralWilds.Economy.WayfarerCommission.WalletOverflowPreventsCompletionWithoutLosingProgress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_WalletOverflow_PreventsCompletionWithoutLosingProgress::RunTest(const FString& Parameters)
{
	FAstralWayfarerCommission Commission;
	FAstralWallet Wallet;
	UAstralEconomyRules::TryRecordAlloySale(Commission, 2);
	UAstralEconomyRules::TryRestoreWallet(Wallet, TNumericLimits<int64>::Max() - UAstralEconomyRules::WayfarerCommissionReward + 1);

	TestFalse(TEXT("Completion fails when the reward would overflow the wallet"), UAstralEconomyRules::TryCompleteCommission(Commission, true, Wallet));
	TestFalse(TEXT("Commission is not marked complete"), Commission.bCompleted);
	TestEqual(TEXT("Sale progress is preserved despite the failed completion"), Commission.AlloySold, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAstralEconomy_Restore_RejectsImpossibleCompletedStateWithoutMutation, "AstralWilds.Economy.WayfarerCommission.RestoreRejectsImpossibleCompletedState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAstralEconomy_Restore_RejectsImpossibleCompletedStateWithoutMutation::RunTest(const FString& Parameters)
{
	FAstralWayfarerCommission Commission;
	UAstralEconomyRules::TryRecordAlloySale(Commission, 1);

	TestFalse(TEXT("Restoring 'completed' with too few sales is rejected"), UAstralEconomyRules::TryRestoreCommission(Commission, 1, true));
	TestEqual(TEXT("Sale count is unchanged by the rejected restore"), Commission.AlloySold, 1);
	TestFalse(TEXT("Completed flag is unchanged by the rejected restore"), Commission.bCompleted);
	TestTrue(TEXT("Restoring 'completed' with enough sales succeeds"), UAstralEconomyRules::TryRestoreCommission(Commission, 3, true));
	TestTrue(TEXT("Completed flag reflects the valid restore"), Commission.bCompleted);
	return true;
}

#endif // WITH_AUTOMATION_TESTS
