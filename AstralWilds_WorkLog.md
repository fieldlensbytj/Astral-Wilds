# Astral Wilds Work Log

Last updated: 2026-09-16

This file records the work performed in the Astral Wilds project, including verified results and remaining limitations. It does not claim runtime success where gameplay input was not actually exercised.

## Project and scene setup

- Confirmed the Unity project at `C:\Users\camer\Astral Wilds`.
- Confirmed Unity `6000.6.0f1`, URP `17.6.0`, Input System `1.20.0`, and Unity AI Assistant/MCP packages.
- Compared `Assets/Astral.unity` with `Assets/Scenes/SampleScene.unity`.
- Configured `Assets/Astral.unity` as the first and only enabled Build Settings scene.
- Kept `Assets/Scenes/SampleScene.unity` in the project but disabled it for builds.
- Confirmed `Assets/Astral.unity` is the active gameplay scene.
- Preserved the third-person player, camera, ground, lighting, and existing beacon objective.

## Beacon and corvette work

- Implemented the original placeholder crashed-beacon objective with Input System interaction, hold-to-activate behavior, activation feedback, and success UI.
- Preserved the interaction marker separately from the visible beacon core.
- Replaced the placeholder crashed-beacon geometry with the approved detailed crashed corvette design.
- Inspected the corvette as an environment asset rather than as an upright beacon.
- Completed conservative cleanup work in Blender according to the approved design constraints; original Meshy GLB/FBX sources were left untouched.
- Prepared the production corvette with weathered gunmetal, muted teal armor, cyan emission materials, UVs, collision proxies, and a Unity-ready FBX.
- Preserved the hull, engines, armor, turrets, antenna tower, proportions, silhouette, and recognizable details.
- Existing corvette assets include:
  - `Assets/Art/Environment/Corvette/Models/CorvetteBeacon_Preview01_Production_v01.fbx`
  - `Assets/Art/Environment/Corvette/Textures/Corvette_BaseColor_2K.png`
  - `Assets/Art/Environment/Corvette/Textures/Corvette_Normal_2K.png`
  - `Assets/Art/Environment/Corvette/Textures/Corvette_MetallicSmoothness_2K.png`
  - `Assets/Art/Environment/Corvette/Textures/Corvette_Emission_Cyan_2K.png`
  - `Assets/Art/Environment/Corvette/Materials/Corvette_WeatheredGunmetal.mat`
  - `Assets/Art/Environment/Corvette/Materials/Corvette_MutedTealArmor.mat`
  - `Assets/Art/Environment/Corvette/Materials/Corvette_CyanEmission.mat`
- Created a versioned scene backup before gameplay-controller integration:
  - `Assets/Astral_BattlePrototypeBackup_v01.unity`

## Astral reference artwork

- Located the exact source folder:
  `C:\Users\camer\OneDrive\Documents\Astral Wilds\Astral_World_Astral_Images`
- Recursively inventoried 19 PNG reference images and visually opened the individual, Main Astral, Legendary Sovereign, and Pseudo-Legendary sheets.
- Selected the six established Main Astral designs for the playable demo reference set:
  - Cindrel
  - Mossling
  - Ripplefin
  - Stormrook
  - Ironbur
  - Glacielle
- Copied three unchanged, SHA-256-verified reference sheets into:
  `Assets/Art/References/Astrals/MainAstrals/`
- The copied sheets were imported by Unity at 1024×1024.
- No matching Astral creature models or textures were found in the project.
- No creature model generation or external artwork upload was performed.
- Legendary and pseudo-legendary references were retained as reserved future references, not selected for the small playable demo set.
- Reference documentation:
  - `Docs/Design/AstralReferenceInventory.md`
  - `Docs/AI/AstralWildsProvenance.md`

## 2v2 battle architecture

- Recorded the corrected proprietary creature term and battle format as **Astral**.
- Implemented data structures for:
  - Six-member player and opponent parties.
  - Two active slots per side.
  - Four maximum active Astrals.
  - Separate reserve collection.
  - Per-slot target, status, position, and queued-action state.
  - Target scopes for one/either/both enemies, one/either/both allies, self, and the entire battlefield.
  - Fainting and battle-end detection.
- Added `AstralCampaignState.cs` for campaign stage, recruitment, reserve overflow, duplicate-reward prevention, and JSON state round-tripping.
- Updated switching so a defeated active slot can be replaced.
- Added `TrySwitchFromReserve` for reserve replacement.
- Prevented a second queued action on the same active slot until the previous queued action is cleared.
- Added EditMode coverage in `Assets/Tests/EditMode/AstralBattleFormatTests.cs`.
- Battle documentation:
  - `Docs/Design/AstralBattleFormat.md`
  - `Docs/AI/AstralWildsProvenance.md`

## Prototype runtime loop

- Added `Assets/Scripts/Battle/AstralDemoLoopController.cs`.
- The controller is a modest, original prototype using the existing Input System and IMGUI.
- Intended controls:
  - `B`: start an encounter from exploration.
  - `Enter`/`B`: begin the encounter battle.
  - `1`/`2`: select an active player slot.
  - `Q`/`W`: select an opponent target.
  - `A`: resolve an attack turn.
  - `R`: recruit after victory or replace from reserve when a slot is fainted.
  - `P`: reorder the party.
  - `K`: save locally.
  - `L`: load locally.
  - `N`: start a protected new game.
  - `E`: remains reserved for the existing beacon objective.
- The controller supports a prototype flow for exploration, encounter, 2v2 battle, target selection, opponent responses, fainting, reserve replacement, victory/defeat, recruitment, party management, and local versioned save/load.
- Unity MCP later compiled the controller, attached it to `Assets/Astral.unity`, and saved the scene.

## Claude Code review attempts

- Initial PATH lookup could not find `claude`.
- The supplied executable was found at:
  `C:\Users\camer\AppData\Local\Microsoft\WinGet\Links\claude.exe`
- Verified version: `2.1.268 (Claude Code)`.
- One earlier wrapper invocation failed with exit code 1 because the prompt was not passed. Captured stderr:
  `Error: Input must be provided either through stdin or as a prompt argument when using --print`
- A corrected direct PowerShell call succeeded:
  - Request: `Reply with exactly READY`
  - Output: `READY`
  - Exit code: `0`
- One focused follow-up JSON review was attempted with read-only tools. It returned empty output and no findings/error payload, so it was not treated as a successful review.
- No Claude-generated changes were applied.

## Verified checks

The following Unity MCP domain smoke checks compiled and executed successfully:

- Party count: 6.
- Seventh Astral: moved to reserve.
- Active battle population: 4.
- Third activation: rejected for both sides.
- Valid target action: queued.
- Fainted active slot: replaced without changing the other active slot.
- All opponent Astrals defeated: battle ended.
- Duplicate reward: rejected.
- Campaign JSON save/load: preserved party state.
- Reserve replacement: removed one reserve entry.
- Duplicate queued action: rejected.
- `CrashedBeaconObjective.beaconCore`: resolved to `BeaconCore_Visible`.
- Player spawn: did not intersect any of the three corvette colliders.
- Scene save/reload: `Assets/Astral.unity` saved and reopened successfully.
- Unity compilation: successful after controller integration.

## Play Mode and visual checks

- Play Mode was entered and exited through Unity MCP.
- A Unity camera capture succeeded and showed the corvette in the scene.
- The available Unity MCP surface did not provide keyboard/gameplay-input injection, so the following were not directly executed end-to-end:
  - Movement, sprinting, jumping, and camera rotation through input.
  - Encounter trigger through keyboard input.
  - Interactive battle actions and target selection through keyboard input.
  - Recruitment and party-management interaction through input.
  - Save, exit, re-enter, and load through the actual runtime UI/input sequence.
  - A second encounter after returning to exploration.
- Therefore the runtime controller is integrated and compiled, but the complete playable sequence remains unverified.

## Console and known warning

- Most Unity MCP checkpoints reported zero errors and zero warnings.
- The latest Console query reported 0 errors and 1 warning:
  `Releasing render texture that is set to be RenderTexture.active!`
- This warning occurred during camera capture and was not attributed to the gameplay scripts.

## Current status

- Verified: project scene/build configuration, reference import, corvette scene integration, beacon-core binding, spawn clearance, domain battle rules, reserve replacement domain behavior, duplicate-action guard, duplicate-reward guard, scene save/reload, and Unity compilation.
- Implemented but unverified: the integrated runtime loop, interactive battle UI, recruitment UI, party management, and persistent runtime save/load.
- Remaining major limitation: actual end-to-end gameplay input verification and built-player validation.
- No Meshy credits were spent in this work.
- No original source artwork, Meshy exports, Blender source scenes, Unity proof-of-concept archives, or reference images were overwritten.

## Cindrel production mesh and Unity prefab

- Audited the untracked Cindrel Meshy source and temporary Blender review scene instead of importing the raw asset directly.
- The raw source was a watertight but production-inappropriate 487,694-vertex / 975,404-triangle GLB with no authored normals, UVs, materials, rig, or animation.
- Added a fail-if-existing Blender normalization recipe and created a separate 35,000-triangle, 1.25 m production mesh with outward normals, smooth shading, one UV set, a grounded origin, and embedded provenance properties.
- Preserved the original GLB and temporary review files without modification.
- Exported and Blender-round-trip validated `Assets/Art/Astrals/Cindrel/Models/Cindrel_Production_v01.fbx`.
- Imported the FBX through Unity's ModelImporter, created an external URP ember-clay review material, a reusable prefab with capsule collider, and a separate validation scene that is not included in Build Settings.
- Captured a real Unity/URP validation render and restored `Assets/Astral.unity` as the active, clean scene without saving any gameplay-scene change.
- Remaining: final reference-faithful texture, quad retopology for deformation, skeleton/skin, animation clips/controller, LODs, gameplay placement, source-prompt recovery, and provider-license confirmation.
- No external generation, remesh, UV, texture, or rigging credits were spent in this pass.

## Spatial wild-encounter site

- Added `Assets/Scripts/Exploration/AstralEncounterZone.cs`, backed by a trigger sphere but using deterministic world-space containment for encounter eligibility.
- Added a visible teal activity marker, four pylons, and a local glow at `(-8, 0, 6)` in `Assets/Astral.unity`, opposite the beacon from the player spawn.
- Replaced the global exploration encounter request with a location gate while keeping both `B` and the clickable HUD action wired to the same battle flow.
- Outside the site, encounter requests stay in Exploration and show guidance. Inside the site, the HUD enables and the existing Encounter -> Battle path proceeds unchanged.
- Added three EditMode tests; the full test assembly passed 14/14.
- Verified the rejection, eligibility, Encounter transition, and Battle transition in Play Mode; returned to a clean saved scene with zero Console errors/warnings.
- No paid provider credits were spent.

## Distinct encounter sites and clear state

- Moved wild-pair identity into `AstralEncounterZone` with stable zone IDs.
- Configured Ember Hollow for Wild Ember/Wild Frost and added a second violet site, Stormbreak Grove, for Wild Stone/Wild Gale.
- Winning a battle now clears the originating site, hides its marker/light, and blocks immediate reuse, requiring travel to another available site.
- Advanced save payloads to v2 with cleared-zone IDs while keeping v1 load compatibility and the existing filename.
- Added clear/reset coverage; the full EditMode assembly passed 15/15.
- Verified a two-site runtime path through the first victory/recruit/return, rejected the cleared site, and began the correct second-site battle. Existing persistent save bytes were left untouched.
- No paid provider credits were spent.

## Exploration objective guidance

- Added a dedicated objective line to the runtime HUD rather than relying on the legacy debug overlay.
- Guidance chooses the nearest uncleared site and reports its name, rounded distance, and eight-way direction.
- The objective automatically advances from Ember Hollow to Stormbreak Grove, then to the crashed beacon, then to a completion message.
- Added six focused direction cases; the complete EditMode assembly passed 21/21.
- Verified the full guidance sequence in Play Mode and confirmed the generated HUD text matches controller state.
- No paid provider credits were spent.

## Demo completion state and economy rule

- Activated the controller's existing `Victory` state when the two-site plus beacon objective completes.
- Added player-facing Continue Exploring, Save Completion, and protected New Expedition choices.
- Continuing acknowledges the completion, unblocks exploration, and prevents the completion state from reopening every frame.
- Codified a hard no-real-money policy in `Docs/Design/EconomyPolicy.md`: every purchase must use currency earned through bosses, item sales, discoveries, quests, or other play rewards.
- Added a manifest regression test rejecting purchasing and ad-monetization packages.
- Full EditMode assembly passed 22/22 after the guard was added.
- No paid provider credits were spent.

## Release UI hardening

- Restricted the legacy IMGUI overlay to Editor/debug managed variants and retained the uGUI as the release-facing interface.
- Replaced obsolete ordered object lookups with `FindAnyObjectByType` in the controller and HUD.
- Full EditMode suite passed 22/22.
- Produced a non-development Windows x64 player and verified the emitted runtime assembly has no `OnGUI` method or prototype debug-title string while retaining the uGUI objective HUD.
- The release build succeeded; its only final warning was from an optional package RuntimePipelineConfig, not first-party game code.
- No paid provider credits were spent.

## Earned-only Starshard economy foundation

- Added an `AstralWallet` domain model with gameplay reward sources, safe spending, item-sale proceeds, save restore, reset, and overflow protection.
- Encounter victories award configured Starshards; Ember Hollow currently grants 50.
- Added a visible gold exploration cache worth 25 Starshards with a stable one-time pickup ID.
- Advanced saves to schema v3 with Starshard balance and collected-cache IDs while retaining v1/v2 compatibility.
- Added the live Starshard balance to the uGUI status line.
- Expanded EditMode coverage to 27/27 passing tests.
- Verified live cache collection, duplicate rejection, encounter reward, final balance 75, reward message, and HUD display.
- No real-money, advertising, premium-currency, or payment integration was added; no provider credits were spent.

## Earned-only inventory and supply relay

- Added Salvaged Alloy and Field Tonics through a bounded `AstralInventory` domain model.
- Added atomic field-vendor transactions: tonics cost 30 earned Starshards and alloy sells for 15; rejected trades leave both wallet and inventory unchanged.
- Encounter victories now yield one alloy. Field Tonics restore 12 HP to the most injured conscious party member and are not consumed when nobody can be healed.
- Added a visible cyan Wayfarer Supply Relay near the crashed beacon with spatial gating, keyboard controls, and clickable HUD actions.
- Advanced saves to schema v4 with an explicit earned-economy snapshot and v1-v3 migration.
- Full EditMode suite passed 34/34. Play Mode verified reward, sale, rejected repeat sale, purchase, healing, and HUD state.
- No real-money, advertising, premium-currency, payment, or paid-provider integration was added.

## Exploration salvage finds

- Added reusable one-time item pickups and two visible Salvaged Alloy scatters in the exploration map.
- Added stable-ID duplicate protection and schema-v5 persistence for collected item finds.
- Full EditMode suite passed 36/36.
- Play Mode verified both finds, marker disappearance, duplicate rejection, HUD inventory, and selling both finds for 30 earned Starshards.
- No paid provider credits were spent.

## Guard action and Stormbreak battle profile

- Added Guard as a real per-slot battle action on keyboard `G` and the clickable HUD.
- Guard consumes the selected Astral's action and reduces its next incoming counterattack to one-third damage, rounded up.
- Added encounter-owned opponent damage and tactical briefing data. Ember Hollow remains balanced at 6; Stormbreak Grove is a 10-damage Stormfront encounter.
- Full EditMode suite passed 43/43.
- Play Mode verified one Stormbreak round at exact HP deltas: unguarded 30 -> 20, guarded 30 -> 26.
- No character art decisions or paid provider calls were involved.

## Arc Burst and readable round results

- Added Arc Burst on `F` and the battle HUD: 8 damage to both opponents versus Attack's focused 12.
- Added explicit counterattack summaries naming each affected party member, actual damage, Guard reduction, and fainting.
- Full EditMode suite passed 44/44.
- Play Mode verified opponents 30 -> 22 each, unguarded party slot 30 -> 24, guarded slot 30 -> 28, and the exact round-summary text.
- No paid provider credits were spent.

## Wayfarer Commission progression

- Added a persistent post-expedition commission for selling two Salvaged Alloy.
- The quest unlocks after the two-site/beacon Victory and pays 40 earned Starshards exactly once.
- Advanced saves to schema v6 with legacy migration and impossible-state validation.
- Full EditMode suite passed 48/48.
- Play Mode verified the complete find -> sell -> two encounters -> beacon -> Victory -> commission path, final balance 170, one-time +40 payout, and objective-HUD completion text.
- No real-money, premium-currency, advertising, or paid-provider integration was added.

## Title, pause, settings, and release shell

- Added a frozen title screen with New Expedition, Continue, Settings, and explicit no-real-money copy.
- Added pause/resume/return-to-title while preserving the active battle or exploration state.
- Added persistent master-volume and look-sensitivity settings applied to the AudioListener and third-person camera.
- Added deterministic first-button focus for keyboard/controller menu navigation and immediate removal of obsolete action buttons.
- Full EditMode suite passed 52/52. Play Mode verified title freeze, input blocking, pause/settings/battle preservation, applied/restored settings, return to title, Continue, and menu focus.
- Fresh non-development Windows x64 build succeeded at 157,132,009 bytes with zero errors. Managed-assembly audit confirmed shell strings present and `OnGUI` absent.
- No paid provider credits were spent.
