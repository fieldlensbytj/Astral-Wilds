# Astral Battle Format

The proprietary creature term is **Astral**.

## Rules

- A trainer party contains up to six Astrals.
- Two party members are active at once; four remain available for switching.
- Standard battles are 2v2: two player Astrals and two opponent Astrals.
- The battlefield therefore has a maximum of four active Astrals.
- Opponent parties may also contain up to six Astrals. Wild encounters may use two wild Astrals when appropriate.
- A seventh captured Astral is placed in the separate reserve collection.
- Each active position is an independent slot. Switching replaces only the selected slot.
- An Astral cannot occupy both active slots, and defeated Astrals cannot return.
- The battle ends when one side has no usable Astrals remaining.

## Data architecture

`AstralParty` owns the six-member party boundary. `AstralReserveCollection` is separate from the party. `AstralBattleState` owns two active slots per side. Each `ActiveAstralSlot` independently tracks its Astral, target slot, status effects through its combatant, position, and queued action.

Target scopes are data-only and intentionally do not implement ability balance or resolution:

- One enemy, either enemy, or both enemies
- One ally, either ally, or both allies
- Self
- Entire battlefield

The final turn-based versus real-time combat model remains an owner decision. This layer preserves the 2v2 slot and targeting contract without choosing timing, ability effects, or balance.

The current playable demo remains turn-based and intentionally provisional. Each conscious active slot currently chooses one action per round: Attack, Guard, or Switch. Guard protects only that field position for the current counterattack phase and reduces incoming damage to one-third, rounded up. Encounter zones may tune opponent damage and provide a tactical brief; this is demo balance, not a commitment to the final combat model.

## UI contract

The future battle UI must show two active player positions, two active opponent positions, six player party slots, active indicators, the selected active slot during switching, and four distinct battlefield target positions. `AstralBattleUiSnapshot` provides the data boundary for those views.

## Validation

EditMode coverage is in `Assets/Tests/EditMode/AstralBattleFormatTests.cs` and covers capacity, reserve overflow, active-slot limits, duplicate occupancy, switching, defeated members, battle end conditions, and UI slot counts.
