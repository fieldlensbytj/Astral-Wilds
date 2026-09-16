# Astral Wilds Economy Policy

## Non-negotiable rule

Astral Wilds has no real-money economy. The game must not contain real-money purchases, premium currency, paywalls, paid progression, paid loot boxes, paid energy, pay-to-skip mechanics, rewarded-ad currency, or links from gameplay to an external checkout.

Every item, upgrade, service, and currency purchase must use value earned by playing the game.

The current earned currency is named **Starshards**.

The first implemented sink is the in-world Wayfarer Supply Relay. Its Field Tonics are purchased only with earned Starshards; Salvaged Alloy obtained from encounter victories can be sold back for Starshards. Transactions are atomic, so rejected trades cannot consume currency or items.

## Allowed currency sources

- Defeating bosses and completing encounters.
- Selling items gathered, crafted, or won in play.
- Finding currency through exploration and random world discoveries.
- Completing quests, objectives, challenges, and optional activities.
- Other gameplay rewards that do not require payment or advertising engagement.

## Allowed currency sinks

- Equipment, consumables, crafting materials, and upgrades.
- In-world services and convenience features purchased entirely with earned currency.
- Cosmetic customization unlocked entirely through play.
- Trading systems whose inputs originate in gameplay.

## Technical guardrails

- Do not add Unity IAP, payment SDKs, store receipt validation, advertising-reward SDKs, premium-currency backends, or checkout links.
- Do not collect payment-card or billing data.
- Do not design two-tier currencies where one tier is purchasable with money.
- Economy balancing may tune play rewards and prices, but must never introduce a payment path.
- Save validation may protect earned balances from corruption; it must not depend on a commercial entitlement service.

## Acceptance test for future economy work

A player starting from a fresh save must be able to obtain every gameplay-affecting item and complete every progression path through play alone, with no purchase prompt or real-world transaction available anywhere in the product.
