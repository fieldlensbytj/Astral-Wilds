namespace AstralWilds
{
    /// <summary>
    /// Earned-currency-only field trades. Preconditions are checked before either
    /// side mutates, making every purchase and sale atomic.
    /// </summary>
    public static class AstralVendorService
    {
        public const long FieldTonicPrice = 30;
        public const long SalvagedAlloySaleValue = 15;

        public static bool TryBuyFieldTonic(AstralWallet wallet, AstralInventory inventory)
        {
            if (wallet == null || inventory == null || wallet.Balance < FieldTonicPrice ||
                !inventory.CanAdd(AstralItemId.FieldTonic, 1))
                return false;

            // Both operations are now guaranteed by the checked preconditions.
            return wallet.TrySpend(FieldTonicPrice) && inventory.TryAdd(AstralItemId.FieldTonic, 1);
        }

        public static bool TrySellSalvagedAlloy(AstralWallet wallet, AstralInventory inventory)
        {
            if (wallet == null || inventory == null ||
                inventory.GetCount(AstralItemId.SalvagedAlloy) < 1 ||
                wallet.Balance > long.MaxValue - SalvagedAlloySaleValue)
                return false;

            // Both operations are now guaranteed by the checked preconditions.
            return inventory.TryRemove(AstralItemId.SalvagedAlloy, 1) &&
                   wallet.TryEarn(SalvagedAlloySaleValue, AstralCurrencySource.ItemSale);
        }
    }
}
