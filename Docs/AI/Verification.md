# Astral Wilds Verification

## 2026-09-17 earned-currency milestone

- Added an earned-only currency named Starshards. `AstralWallet` supports gameplay rewards, affordable positive spending, item-sale proceeds, restore/reset, and overflow rejection; it contains no payment or premium-currency concepts.
- Encounter-site victory awards the site's configured clear reward (currently 50 Starshards).
- Added a gold exploration cache at `(-4, 0.7, -5)` worth 25 Starshards. Stable pickup IDs prevent duplicate collection and drive saved world state.
- Save schema v3 adds `starshards` and `collectedCurrencyPickupIds`; v1/v2 remain accepted with zero balance/no collected caches.
- Runtime HUD state line includes the live Starshard balance.
- Full EditMode assembly: 27 passed, 0 failed, 0 skipped.
- Live Play Mode verified: exploration cache 0 -> 25; second collection rejected; cache hidden; Ember Hollow victory 25 -> 75; reward message correct; generated HUD refreshed to `Starshards 75`.
- Existing persistent-save bytes were deliberately not overwritten, so this pass did not perform a disk round-trip of schema v3. Domain restore and validation paths are covered and compiled.
- Final audit: Edit Mode, one active uniquely identified cache, `Assets/Astral.unity` clean, 0 Console errors / 0 warnings.
- No paid or generative provider calls were used.

## 2026-09-17 earned-only vendor and inventory milestone

- Added `AstralInventory` for Salvaged Alloy and Field Tonics plus `AstralVendorService` with preflighted, atomic transactions.
- Encounter victories now grant one Salvaged Alloy alongside the configured Starshard reward.
- Added the cyan Wayfarer Supply Relay at `(-2, 0, 5)`. It buys Field Tonics for 30 earned Starshards and buys Salvaged Alloy from the player for 15 Starshards.
- Field Tonics heal 12 HP on the most injured conscious party member and are consumed only when healing is possible.
- Advanced saves to schema v4 using `AstralEconomySaveData`; v1-v3 saves explicitly migrate their legacy Starshard balance and begin with zero inventory.
- Full EditMode assembly: 34 passed, 0 failed, 0 skipped. Coverage includes successful and failed atomic trades, overflow rejection, inventory validation, and JSON economy roundtrip.
- Play Mode verified an actual encounter award (50 Starshards + 1 alloy), relay proximity, sale to 65 Starshards, rejected duplicate sale with no mutation, tonic purchase to 35 Starshards, HUD vendor actions/counts, and tonic healing on an injured party member.
- No real-money, premium-currency, advertising, checkout, or payment system exists. No paid or generative provider calls were used.

## 2026-09-17 exploration salvage milestone

- Added reusable `AstralItemPickup` world finds with stable IDs, proximity collection, inventory-capacity checks, and one-time disappearance.
- Added two glowing Salvaged Alloy scatters at `(-5, 0.35, 2)` and `(6, 0.35, -3)`.
- Advanced saves to schema v5 with collected-item pickup IDs while retaining v1-v4 compatibility.
- Full EditMode assembly: 36 passed, 0 failed, 0 skipped.
- Play Mode verified two finds collected, both markers hidden, duplicate collection rejected, inventory reaching two alloy, and both sold at the relay for 30 Starshards. The HUD matched `Starshards 30 / Alloy 0 / Tonics 0`.
- No paid or generative provider calls were used.

## 2026-09-17 Guard and Stormbreak tactics milestone

- Added a Guard action for either active player slot through keyboard `G` and the clickable battle HUD.
- Guard consumes that slot's round action and reduces its next incoming counterattack to one-third, rounded up, with a minimum of one damage.
- Encounter zones now own battle profiles. Ember Hollow remains at 6 counter-damage; Stormbreak Grove is a 10-damage Stormfront encounter and tells the player to guard or rotate injured Astrals.
- Full EditMode assembly: 43 passed, 0 failed, 0 skipped. Pure combat-rule cases cover guarded, unguarded, zero, and negative damage inputs.
- Play Mode verified a mixed Attack/Guard Stormbreak round: the unguarded active slot moved from 30 to 20 HP and the guarded slot from 30 to 26 HP. The state remained Battle and the Guard HUD action was present.
- No paid or generative provider calls were used.

## 2026-09-17 Arc Burst and round-feedback milestone

- Added Arc Burst on keyboard `F` and the clickable battle HUD. It deals 8 damage to every living opponent, trading Attack's 12 focused damage for greater two-target pressure.
- Counterattack resolution now reports each party member hit, actual damage, whether Guard reduced it, and whether the member fainted.
- Full EditMode assembly: 44 passed, 0 failed, 0 skipped. The action-value test locks the intended target tradeoff: per-target Burst damage is lower than Attack while its two-target total is higher.
- Play Mode verified an Ember Hollow Burst/Guard round: both opponents 30 -> 22; unguarded Cindrel 30 -> 24; guarded Mossling 30 -> 28. The exact round summary rendered in controller state and the Arc Burst HUD action was present.
- No paid or generative provider calls were used.

## 2026-09-17 Wayfarer Commission milestone

- Added `AstralWayfarerCommission`, a persistent post-expedition quest that records two Salvaged Alloy sales and awards 40 gameplay-earned Starshards once the main two-site/beacon expedition is acknowledged.
- The objective HUD hands off from the main expedition to live commission progress and then a completion receipt.
- Advanced saves to schema v6 with v1-v5 compatibility and validation that rejects negative sales or a completed commission without the required sales and acknowledged expedition.
- Full EditMode assembly: 48 passed, 0 failed, 0 skipped. Tests cover locked completion, one-time payout, insufficient sales, wallet overflow, and impossible restore state.
- Play Mode verified the complete extended loop: two exploration finds -> two relay sales (30 Starshards) -> both encounter victories (+100) -> beacon/Victory -> Continue -> one 40-Starshard quest payout. Final balance was 170, repeated continuation did not pay again, and the live objective HUD read `Wayfarer Commission complete: earned 40 Starshards.`
- No paid or generative provider calls were used.

## 2026-09-17 title, pause, and settings shell milestone

- Added a frozen title screen with New Expedition, Continue, Settings, and an explicit statement that Astral Wilds contains no real-money purchases, premium currency, or advertising.
- Continue uses the existing validated/migrated save path. New Expedition resets runtime progression without deleting the disk checkpoint.
- Added pause/resume from every gameplay flow, safe-checkpoint save from pause, settings access, and return to title. The active battle state remains intact across pause/settings/resume.
- Added persistent five-step master volume (0-100%) and look sensitivity (50-150%) settings, applied to `AudioListener.volume` and the existing third-person camera.
- Action-bar rebuilds detach obsolete buttons immediately and select the first interactable action, giving keyboard/controller navigation deterministic focus.
- Full EditMode assembly: 52 passed, 0 failed, 0 skipped. Settings coverage includes defaults, wraparound, JSON roundtrip, and invalid-state rejection.
- Play Mode verified: Title state, `Time.timeScale = 0`, world input blocked, title copy/actions rendered, New Expedition unfreezes gameplay, Battle -> Pause -> Settings -> Pause -> Battle preserves opponent HP, settings applied as `0.00` volume / `1.25` look scale and restored to 100/100, Return to Title re-freezes input, Continue loaded the existing checkpoint, and EventSystem selected `Btn_New Expedition (Enter)`.
- Fresh Windows x64 release build (`BuildOptions.None`): succeeded, Development false, 157,132,009 bytes, 0 errors. Mono.Cecil confirmed `ASTRAL WILDS`, New Expedition, and the no-real-money copy are present while `OnGUI` remains absent. Build warnings came from package shader variants and the known optional RuntimePipelineConfig warning.
- No paid or generative provider calls were used.

## 2026-09-17 release UI hardening milestone

- Legacy IMGUI diagnostics now compile only in Editor/debug managed-code variants; release players use the generated uGUI exclusively.
- Replaced all seven obsolete `FindFirstObjectByType` calls in first-party runtime scripts with unordered `FindAnyObjectByType` lookups.
- Full EditMode assembly: 22 passed, 0 failed, 0 skipped.
- Built a Windows x64 player with `BuildOptions.None`: succeeded, Development flag false, 157,092,965 bytes.
- Inspected the emitted `AstralWilds.Runtime.dll` with Mono.Cecil: `AstralDemoLoopController.OnGUI` absent; `ASTRAL WILDS PROTOTYPE` string absent; `AstralBattleHUD`'s `ObjectiveLine` string present.
- Final build emitted no first-party script warnings. One package-level warning remained because no optional RuntimePipelineConfig asset is configured.
- Active scene remained `Assets/Astral.unity`, clean, in Edit Mode.
- No paid or generative provider calls were used.

## 2026-09-17 completion-state and economy-policy milestone

- Completed objectives now enter the existing `Victory` flow and block exploration input.
- Runtime HUD exposes Continue Exploring, Save Completion, and New Expedition. New Expedition uses the existing confirm/cancel restart gate.
- Continue acknowledges the completion, returns to free exploration, unblocks the player, and does not reopen Victory on the next frame.
- Save payloads retain the completion acknowledgement so a continued completed save can remain in free exploration after load.
- Added `Docs/Design/EconomyPolicy.md`: no real-money purchase, premium currency, paywall, paid progression, paid loot box, paid energy, pay-to-skip, rewarded-ad currency, or checkout integration is allowed.
- Added an EditMode guard against purchasing and ad-monetization packages in `Packages/manifest.json`.
- Full EditMode assembly after adding the guard: 22 passed, 0 failed, 0 skipped.
- Live Play Mode verification confirmed Victory entry, input blocking, all three choice labels, protected restart/cancel, Continue, and stable post-Continue exploration.
- No paid or generative provider calls were used.

## 2026-09-17 exploration-guidance milestone

- Added a cyan objective line to the runtime HUD status panel.
- While encounters remain, guidance selects the nearest available site and shows its name, rounded horizontal distance, and eight-way world direction.
- After the first site clear, the line switches from Ember Hollow to Stormbreak Grove. After two clears, it points to the crashed beacon and reminds the player to hold Interact. Beacon activation changes it to an objective-complete message.
- Added direction-mapping tests for cardinal, diagonal, and near-target cases. Full EditMode assembly: 21 passed, 0 failed, 0 skipped.
- Live Play Mode progression produced: `Ember Hollow: 10 m northwest` -> `Stormbreak Grove: 21 m southeast` -> `crashed beacon: 15 m northwest` -> objective complete. The generated uGUI `ObjectiveLine` matched the controller's initial guidance exactly.
- Final Unity audit: Edit Mode, `Assets/Astral.unity` active and clean, 0 Console errors / 0 warnings.
- No paid or generative provider calls were used.

## 2026-09-17 multi-site encounter milestone

- Encounter zones now own stable IDs plus their two wild Astral IDs/names; the battle controller no longer rotates a global preset list.
- Configured Ember Hollow at `(-8, 0, 6)` with Wild Ember/Wild Frost and Stormbreak Grove at `(9, 0, -7)` with Wild Stone/Wild Gale.
- A victory marks the active site cleared, disables its renderers/light, and prevents another encounter there. New game resets all sites.
- Save schema advanced to v2 with `clearedEncounterZoneIds`; v1 saves remain accepted and load with no sites cleared. The persistent filename is unchanged for backward compatibility.
- Full EditMode assembly: 15 passed, 0 failed, 0 skipped.
- Live Play Mode sequence verified: enter Ember Hollow; confirm Ember/Frost; win; confirm site and visuals cleared; recruit; return to Exploration; confirm cleared site rejected; travel to Stormbreak Grove; confirm Stone/Gale; begin second Battle.
- The live probe deliberately did not overwrite the existing persistent save file. Save/load code compiled and the new field is validated, but this pass did not perform a disk round-trip of v2 progress.
- Final Unity audit: Edit Mode, two unique available scene-authored sites, clean active scene, 0 Console errors / 0 warnings.
- No paid or generative provider calls were used.

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
