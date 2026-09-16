# Astral Wilds Full Task Audit

Audit date: 2026-09-16. Evidence is separated between repository inspection, Unity MCP inspection, and executed runtime tests.

## Requirement matrix

| Requirement | Status | Evidence |
|---|---|---|
| Approved corvette source preserved | Implemented but unverified | Production FBX exists at `Assets/Art/Environment/Corvette/Models/CorvetteBeacon_Preview01_Production_v01.fbx`; no Blender MCP inspection was performed in this audit. |
| Corvette textured production maps | Implemented but unverified | Four 2K maps and three Unity materials exist, plus `Backup_v01_uniform`; no fresh visual render was executed in this audit. |
| Corvette exported and integrated | Implemented and verified | Unity MCP hierarchy showed `CrashedCorvette` with three BoxColliders and `CorvetteVisual` in `Assets/Astral.unity`. |
| Beacon objective and Input System prompt | Implemented but unverified | `CrashedBeaconObjective.cs` and `BeaconObjectiveUI.cs` exist; scene contains `CrashedBeaconObjective`, marker, light, visible core, and UI. Full input hold was not executed. |
| Visible `beaconCore` reference | Implemented but unverified | Scene contains `BeaconCore_Visible`; serialized field binding was not independently read from the scene in this audit. |
| Six-member party / two active per side / four active maximum | Implemented and verified (domain) | `AstralBattleFormat.cs` enforces capacities and slot limits; Unity MCP domain smoke test compiled and executed with `active=4` and both third-slot activations rejected. Existing EditMode tests were not run through Test Runner. |
| Switching and fainted-slot replacement | Implemented and verified (domain) | Unity MCP domain smoke test marked player slot 0 defeated, replaced it with `p2`, and preserved slot 1 as `p1`. |
| Reserve overflow and duplicate reward prevention | Implemented and verified (domain) | Unity MCP domain smoke test recruited seven members: party `6`, reserve `1`, duplicate result `DuplicateReward`. |
| Reserve replacement during battle | Implemented and verified (domain) | `TrySwitchFromReserve` replaces a populated active slot and removes the selected reserve entry; Unity MCP smoke test passed with reserve count reduced from 3 to 2. |
| Exploration → encounter → 2v2 battle | Missing | No encounter manager, battle runtime controller, battle UI, or scene wiring exists. |
| Recruitment and party-management UI | Missing | No runtime recruitment flow or party UI exists; only data structures and a UI snapshot contract exist. |
| Save / reload campaign flow | Implemented but unverified | Unity MCP scene save/reload executed successfully for `Assets/Astral.unity`; domain smoke test round-tripped campaign JSON with stage `ReturnToExploration`, beacon `True`, party `6`, reserve `1`. No file-backed runtime save service or UI exists. |
| Full requested play sequence | Blocked | Unity MCP entered Play Mode and exited cleanly, but provides no input/playback control; the project also lacks the required encounter/battle/recruitment systems. |
| Established reference images inspected and used | Implemented and verified | 19 source PNGs were recursively inventoried and visually inspected; six Main Astrals were selected. Three hash-matched sheets were copied to `Assets/Art/References/Astrals/MainAstrals/` and imported by Unity at 1024×1024. |
| No crafting | Implemented and verified | No crafting code or assets found; design documentation explicitly preserves the constraint. |
| Console clean | Implemented and verified at audit checkpoints | Unity MCP returned 0 errors and 0 warnings before the bridge disconnected. Post-repair Console could not be queried. |

## Executed versus inferred tests

Executed:

- Unity MCP scene traversal: active scene `Assets/Astral.unity`; seven roots; player, third-person camera, corvette, objective marker/core/light, and objective UI present.
- Build Settings inspection: `Assets/Astral.unity` enabled at index 0; `Assets/Scenes/SampleScene.unity` disabled.
- Unity MCP compile/refresh of reference images: all three imported at 1024×1024.
- Unity MCP Play Mode entry and exit: both commands returned successfully; no gameplay inputs were supplied.
- Unity Console query before the battle-domain repair: 0 errors, 0 warnings.

Not executed:

- Unity EditMode test runner after the repair.
- Any actual movement, sprint, jump, camera rotation, encounter, battle, recruitment, switching, fainting, save, reload, or duplicate-reward runtime test.
- Blender renders or mesh inspection in this audit.
- A player build.

The 2v2, recruitment, and campaign JSON claims are executed domain evidence, not full Play Mode evidence.

## Repair performed

- Added `Assets/Scripts/Battle/AstralCampaignState.cs` with six-party capacity, reserve overflow, duplicate-reward prevention, stage progression, and JSON save/load boundary.
- Updated `AstralBattleState.TrySwitch` so a defeated active slot can be replaced without affecting the other active slot.
- Added focused EditMode coverage for fainted-slot replacement, recruitment overflow/duplicate rewards, and save/reload state preservation.

Remaining priority: implement and wire the encounter/battle/recruitment/party UI and file-backed runtime save flow, then validate the complete sequence through actual Play Mode input.
