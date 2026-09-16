# Astral Wilds Verification

## 2026-09-17 spatial encounter milestone

- Added a reusable `AstralEncounterZone` whose eligibility is calculated from its world-space sphere, independent of trigger-callback timing.
- Added a visible teal activity marker and trigger volume at `(-8, 0, 6)` in `Assets/Astral.unity`; the player spawn at `(0, 0, 0)` is outside the radius.
- Preserved both encounter entry points: keyboard `B` and the clickable HUD action. The HUD action is disabled and labelled `Find wild activity` outside the site, then becomes `Encounter (B)` inside it.
- Outside-site requests remain in Exploration and provide navigation guidance. Inside-site requests advance through the existing Encounter and Battle states without changing the verified 2v2 rules.
- Ran the complete EditMode assembly synchronously: 14 passed, 0 failed, 0 skipped. Three new tests cover inside/outside bounds, world scale, and trigger configuration.
- Play Mode probe verified: rejected at spawn, eligible after entering the site, Encounter started, then 2v2 Battle started.
- Final Unity audit: Editor returned to Edit Mode, `Assets/Astral.unity` active and clean, site present, no temporary validation camera, and Console at 0 errors / 0 warnings.
- No paid or generative provider calls were used.

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

## 2026-09-16 Fainted-slot replacement and built-player verification (Cowork)

- `R` as a fainted-slot replacement in battle: verified with a genuine forced faint (not the unreachable `ForceSelectedFaintForPrototypeTesting()`). Left an active party member at partial HP after one battle (HP persists across encounters -- confirmed via Debug Inspector), then a second battle's opponent counter-fire (6 HP/round) fainted that active slot. Pressing `R` correctly invoked `ReplaceFaintedFromReserve() -> SwitchToBench(true)`, pulling a healthy reserve member into the empty active slot. Confirmed via Debug Inspector.
- Built-player (non-Editor) smoke test: completed successfully, after fixing a real build-breaking bug found in the process.
  - Root cause: the project had no `.asmdef` files, so the NUnit-based EditMode test file (`Assets/Tests/EditMode/AstralBattleFormatTests.cs`) compiled into the default player-included `Assembly-CSharp`, and the Unity CIL Linker failed to resolve `nunit.framework` for the player build.
  - Fix: added `Assets/Scripts/AstralWilds.Runtime.asmdef` (runtime code; included in player builds, references `Unity.InputSystem` and `UnityEngine.UI`) and `Assets/Tests/EditMode/AstralWilds.Tests.EditMode.asmdef` (`includePlatforms: ["Editor"]`, references the Runtime asmdef plus `UnityEngine.TestRunner`/`UnityEditor.TestRunner`/`nunit.framework.dll`).
  - That split broke the test file's access to `internal` members of `AstralReserveCollection` (CS1061 on `TryAdd`); fixed with `Assets/Scripts/AssemblyInfo.cs` adding `[assembly: InternalsVisibleTo("AstralWilds.Tests.EditMode")]`. No behavior changed; this only restores cross-assembly visibility that existed implicitly when everything compiled into one assembly.
  - Rebuilt (Windows Build Profile, Local Machine, "Build And Run"): succeeded in 42 seconds. Confirmed via `find` that `Astral Wilds.exe` and its `_Data` folder exist on disk.
  - Launched the standalone `.exe` directly (outside the Editor): started cleanly, rendered the exploration scene (crashed corvette, terrain, lighting) matching the Editor's Play Mode view, and the on-screen debug overlay showed correct state (`Party: 5/6  Reserve: 0`, five Astrals each at full HP). Mouse-look input was confirmed to move the camera, i.e. the build is genuinely interactive, not a frozen frame.
  - The player process was left running rather than force-closing it via a system-level shortcut (Alt+F4 requires an OS-level permission grant this session didn't have); it's safe to close manually.
- Console: 0 errors after the asmdef fix and Assets > Refresh (was showing compile errors immediately after the split, before `AssemblyInfo.cs` was added).

### Still unverified or blocked

- `N` restart (not separately re-tested this session; not expected to be affected by any change made).
- A true Editor exit/re-entry save reload (still only verified within a continuous Play Mode session).
- Performance/frame-rate characteristics of the built player (not measured; only startup and basic responsiveness were checked).

## 2026-09-16 Real clickable battle/party UI (Cowork)

Per the vision doc's top-priority next step ("replace keyboard-only shortcuts with a readable battle and party interface using the approved portraits"):

- Cropped 5 in-game creature portraits (Cindrel, Mossling, Ripplefin, Stormrook, Ironbur) out of the three approved reference sheets (`Assets/Art/References/Astrals/MainAstrals/*.png`) via a Python/Pillow script, saved to `Assets/Resources/Portraits/`. An `AssetPostprocessor` (`Assets/Editor/AstralPortraitImporter.cs`, in a new Editor-only `AstralWilds.Editor.asmdef`) force-imports anything under that folder as a UI Sprite automatically.
- Added a purely additive public API to `AstralDemoLoopController` (`GetPartyUiInfo()`, `GetOpponentUiInfo()`, `Ui*` action methods mirroring every existing keyboard handler) -- no existing private logic was changed, so all prior keyboard-driven verification still holds unmodified.
- Added `Assets/Scripts/UI/AstralBattleHUD.cs`: a self-installing (`[RuntimeInitializeOnLoadMethod]`, no scene/prefab edit needed) uGUI battle and party interface. Party row (up to 6 slots) and opponent row (2 slots, battle-only) show portrait, name, an HP bar, fainted tinting, and active/target highlighting; a context-sensitive action bar shows only the buttons valid for the current state (Encounter/Party/Save/Load/Restart while exploring; Attack/Replace Fainted/Swap in battle; Recruit after victory; Reorder/Return in party management; Recover on defeat; Confirm/Cancel for restart). Clicking a party or opponent portrait selects it as the active/target slot.
- Verified live in Play Mode via computer-use clicks (not keyboard): Exploration -> Encounter -> Battle -> clicked Attack twice (damage applied correctly, turn correctly advanced to the second active slot) -> clicked an opponent portrait to retarget -> fainted-tint rendered correctly on a defeated opponent -> Victory -> clicked Recruit (6th party member added, shown with a placeholder portrait since it has no approved art) -> Party Management -> back to Exploration. Every state transition and damage number matched the already-verified keyboard-driven battle rules exactly, confirming the new UI is a faithful additive front-end and not a rules change.
- The original OnGUI debug overlay (top-left text box) was left in place as a secondary ground-truth readout; it is now redundant for normal play but still useful for debugging.
- Compiled with 0 errors (9 pre-existing `FindFirstObjectByType` obsolete-API warnings, unrelated to this change).

## 2026-09-16 Encounter variety and demo-objective completion (Cowork)

Per the vision doc's opening-demo proposal ("a second encounter should encourage a different pair choice" and "returning to the corvette completes the short objective"):

- `AstralDemoLoopController.BeginBattle()` now picks the wild opponent pair from a small rotating preset list keyed off `encountersCompleted`, instead of always spawning the same Ember/Frost pair. Verified live: encounter 1 = Wild Ember Astral + Wild Frost Astral; after winning and recruiting, encounter 2 = Wild Stone Astral + Wild Gale Astral, a genuinely different pair.
- Added `DemoObjectiveComplete` (`BeaconActivated && encountersCompleted >= 2`) to `AstralDemoLoopController`, and a banner in `AstralBattleHUD` that appears once it's true. The existing `CrashedBeaconObjective`/`BeaconObjectiveUI` proximity-and-hold-to-activate system (already implemented, not modified) was reused as-is rather than rebuilt.
- Verified the full short-demo loop live: encounter 1 -> battle -> recruit -> encounter 2 (different pair) -> battle -> recruit -> walked the player to the crashed corvette (held W) -> held E to activate the beacon -> both "BEACON ONLINE" (existing) and "DEMO OBJECTIVE COMPLETE" (new) banners displayed correctly together.
- No spatial gating was added to encounter triggers (B still works from anywhere, unchanged) -- a location-gated encounter zone (mirroring the beacon's own interactionRadius pattern) is a reasonable next step but was left out this session to avoid touching verified movement/collision behavior without first inspecting the terrain collision layout carefully.

## 2026-09-16 Cindrel production-mesh milestone

- Preserved the original Meshy GLB byte-for-byte and created a deterministic Blender 5.2 normalization recipe at `ArtSource/Blender/normalize_cindrel.py`.
- Reduced the source from 487,694 vertices / 975,404 triangles to 17,492 vertices / 35,000 triangles while retaining a closed manifold surface (0 boundary edges, 0 non-manifold edges, 0 loose vertices/edges).
- Normalized to 1.25 m tall, grounded and centered the mesh, recalculated outward normals, generated one UV set, added a review material, and exported a Unity-axis FBX.
- Re-imported the FBX into Blender and verified identical topology, dimensions, transform, UVs, material, and provenance properties.
- Imported the FBX into Unity 6000.6.0f1 with normals imported, Mikk tangents calculated, mesh optimization enabled, read/write disabled, animation disabled, and no mesh compression.
- Created `Cindrel_EmberClay.mat`, `Cindrel_Production_v01.prefab`, and the build-excluded `CindrelValidation.unity` scene. The prefab has one correctly assigned renderer and a bounds-derived capsule collider.
- Rendered and visually inspected Cindrel through a real Unity camera/URP material. The fox-like head, oversized ears, layered fur, quadruped stance, and curled/flame-like rear silhouette remain recognizable after reduction.
- Unity audit confirmed 35,000 triangles, required assets/references present, validation scene excluded from Build Settings, and `Assets/Astral.unity` active and clean afterward.
- Limitations: placeholder single-color material, no deformation-ready quad retopology, rig, skin, animation, LOD chain, or gameplay placement. Provider license and the missing original generation prompt remain unresolved; no paid credits were spent.
