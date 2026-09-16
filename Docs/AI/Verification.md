# Astral Wilds Verification

## 2026-09-16 checkpoint

### Verified by executed Unity MCP domain smoke test

- Party count reached 6.
- Seventh Astral reached reserve.
- Two active player and two active opponent slots produced 4 total active Astrals.
- Third activation was rejected on both sides.
- Target action queue accepted a valid target scope.
- Fainted active slot was replaced without changing the other active slot.
- All opponent Astrals fainting ended the battle state.
- Duplicate reward was rejected.
- JSON campaign round-trip preserved a six-member party.
- Reserve replacement removed one reserve entry.
- Duplicate action queueing on one active slot was rejected.

### Current unverified or blocked

- Unity MCP later reconnected; `AstralDemoLoopController.cs` compiled successfully, was attached to `Assets/Astral.unity`, and the scene was saved.
- Exploration encounter, interactive battle UI, opponent turns, recruitment UI, party management, persistent file save/load, second encounter, and return-to-exploration have not been verified through gameplay input.
- No built executable test was performed.
- Play Mode was entered and exited, but keyboard gameplay input could not be injected through the available MCP surface.

### Tool evidence

- Claude executable version: `2.1.268`.
- Corrected Claude diagnostic: direct PowerShell call returned `READY`, exit code 0.
- The single focused follow-up JSON review returned empty output and no findings/error payload; it is not review evidence.
- Latest Console query: 0 errors, 1 warning (`Releasing render texture that is set to be RenderTexture.active!`) associated with camera capture.

## 2026-09-16 Play Mode verification (Cowork, real keyboard input via computer-use)

### Verified through actual gameplay input (not domain-only)

- Exploration -> Encounter via `B`.
- Encounter -> Battle via `Enter`.
- Active slot selection (`1`/`2`), target selection (`Q`/`W`), attack (`A`): correct 12 HP damage per hit, correct defeat-at-0 detection.
- Opponent counter-damage (6 HP/round to a player active slot) confirmed firing.
- Voluntary bench swap (`S`) confirmed, independent of the fainted-slot replacement path.
- Battle victory -> Recruitment transition confirmed.
- Recruit (`R`): confirmed adding to party while under capacity, and correctly overflowing to reserve once party reached 6/6.
- Party management entry (`P`) confirmed from both Exploration and post-recruit.
- Return to Exploration (`Enter`) confirmed from Party Management.
- Save (`K`) and load (`L`) confirmed together: saved in Exploration, state was changed (new Encounter started), then `L` correctly reverted to the saved Exploration checkpoint.
- Two full encounter/battle/recruit/save cycles run back to back; `encountersCompleted` correctly incremented 1 -> 2.
- Console: 0 errors, 0 warnings throughout this run (no render-texture warning this time; that earlier warning was tied to an MCP camera capture, not gameplay).

Ground truth for this run was the component's Debug Inspector (`flow`, `message`, etc. as live private-field values), not the tiny on-screen IMGUI text, after the latter proved unreliable to read via screenshot at the Game view's default scale.

### Still unverified or blocked

- `R` as a fainted-slot replacement in battle specifically (voluntary `S` swap was verified instead; reaching a real faint requires 5 rounds of opponent counter-damage on one slot).
- `N` restart.
- A built-player (non-Editor) test.
- A true Editor exit/re-entry save reload (this run verified save/load within one continuous Play Mode session).

See `Docs/AI/CoworkReview-20260916-PlayModeVerified.md` for full narrative detail.
