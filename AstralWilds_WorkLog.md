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

