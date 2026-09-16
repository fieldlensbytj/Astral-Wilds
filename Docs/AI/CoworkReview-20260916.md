# Cowork Code Review — 2026-09-16

Author: Claude (Cowork session), reviewing alongside Claude Code / Codex. Read-only pass — no gameplay code changed. Logged here rather than in WorkQueue.md/Verification.md to avoid overwriting an in-progress handoff.

## Setup done this session

- Initialized a git repository at project root (`git init`, `.gitignore` matching `ignore.conf`'s intent) and made an initial baseline commit (`eca3454`, 188 tracked files) so any agent's edits from here on are recoverable.

## Findings from reading Assets/Scripts

### 1. The tested campaign system and the playable one are different, disconnected pieces of code

`Assets/Scripts/Battle/AstralCampaignState.cs` (`AstralCampaignState`/`AstralMemberRecord`, with `SaveJson`/`LoadJson`) is the only campaign-state code covered by `Tests/EditMode/AstralBattleFormatTests.cs`, and it's what `Verification.md`'s "JSON campaign round-trip preserved a six-member party" line is verifying.

But the actual playable loop, `AstralDemoLoopController.cs` (attached to `Assets/Astral.unity`), never references `AstralCampaignState` — it has its own parallel `DemoMember` / `SaveData` classes and its own `SaveGame()`/`LoadGame()` using `Application.persistentDataPath`. The only place it even touches `AstralCampaignState` is reading the `PartyCapacity` constant (line 361).

Net effect: the save/load path a player would actually hit has no test coverage, and the tested save/load path isn't reachable in play. Worth deciding whether `AstralCampaignState` is meant to replace the demo loop's ad hoc save system later, or should be deleted to avoid the two drifting further apart.

### 2. Party members that faint mid-battle (but the team still wins) never heal

Searched all `.hp =` / `.defeated = false` sites in `AstralDemoLoopController.cs`. The only place fainted members are restored is inside `ReturnToExploration()`'s `Flow.Defeat` branch (full team wipe). `CompleteBattle(true)` (victory), `RecruitReward()`, and the non-defeat branch of `ReturnToExploration()` never touch `hp`/`defeated`.

So an Astral that faints mid-fight in a battle you otherwise win is permanently benched — there's no healing trigger short of losing every Astral at once. Over a few encounters this can quietly strand most of the party. This may be intentional for a prototype (no camp/heal system yet), but it reads as a gap rather than a design choice, so flagging it rather than assuming.

### 3. Dead debug hook

`ForceSelectedFaintForPrototypeTesting()` (lines 323–331) is fully implemented but not wired to any key in `Update()`/`UpdateBattleInput()`. It looks like it was meant to let a tester force a faint without playing out full combat, which would be directly useful for the blocked "exercise full keyboard sequence" verification — it's just never called.

### 4. Minor naming mismatch

`ReplaceFaintedFromReserve()` (R key, in battle) actually calls `SwitchToBench(true)`, which pulls from the live `party` bench slots, not the `reserve` collection. Functionally fine (the two-tier party/reserve split is preserved correctly elsewhere), just a naming trap for future maintainers — R does something different depending on flow (Recruit in `Flow.Recruitment`, bench-swap in `Flow.Battle`).

### What looked solid

`AstralBattleFormat.cs` (`AstralBattleState`/`AstralParty`/slots) is clean, matches its test coverage well, and the duplicate-action/duplicate-activation guards are correct. The `InputBlocked` gating pattern across `AstralPlayerController`, `AstralThirdPersonCamera`, and `CrashedBeaconObjective` is consistent and correctly suspends exploration input during battle/menus/restart-confirm.

## Where I think I can add the most value next

The current blocker in `WorkQueue.md` is that the Unity MCP surface can't inject keyboard input, so the full B → encounter → 2v2 battle → recruit → save/load sequence has never been exercised end-to-end in Play Mode. `AstralDemoLoopController` reads raw keys via `Keyboard.current` (B/Enter/1/2/Q/W/A/R/S/P/K/L/N), independent of the serialized Input Actions asset — which means literal keyboard/mouse control of the running Editor (not MCP) could actually drive this sequence and observe results. That's a capability this session has (desktop computer control) that the existing pipeline doesn't. Flagging this as the concrete next step to discuss with the user.
