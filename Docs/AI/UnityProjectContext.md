# Unity Project Context

<!-- unity-onboarding:generated:start -->

## Project Summary

- Project root: `C:\Users\camer\Astral Wilds`
- Last analyzed: 2026-09-15
- Last analyzed commit: unavailable; workspace is not a Git repository

## Confirmed Environment

- Unity version: 6000.6.0f1
- Render pipeline: Universal Render Pipeline 17.6.0
- Input system: Unity Input System 1.20.0 using `Assets/InputSystem_Actions.inputactions`
- Target platforms: unresolved; project contains desktop, mobile, WebGL, and XR modules

## Important Packages And Frameworks

| Area | Finding | Confidence | Evidence |
| --- | --- | --- | --- |
| Rendering | URP with PC/mobile renderer assets | Confirmed | `Packages/manifest.json`, `Assets/Settings/` |
| Input | Input System with `Player` and `UI` action maps | Confirmed | `Packages/manifest.json`, `Assets/InputSystem_Actions.inputactions` |
| Navigation | Unity AI Navigation 2.0.14 installed; no project navigation code yet | Confirmed | `Packages/manifest.json`, `Assets/` inspection |
| Testing | Unity Test Framework installed; no first-party tests found | Confirmed | `Packages/manifest.json`, asset scan |

## Directory Structure

| Path | Purpose | Confidence | Evidence |
| --- | --- | --- | --- |
| `Assets/Astral.unity` | Active gameplay scene | Confirmed | Unity MCP scene inspection |
| `Assets/Scenes/SampleScene.unity` | Preserved template scene, disabled for builds | Confirmed | Unity MCP/build settings |
| `Assets/Settings/` | URP and volume assets | Confirmed | asset scan |
| `Assets/Scripts/` | Runtime gameplay scripts | Confirmed | feature implementation |
| `Assets/Scripts/Objectives/` | Beacon objective logic and uGUI presentation | Confirmed | `CrashedBeaconObjective.cs`, `BeaconObjectiveUI.cs` |
| `Assets/TutorialInfo/` | Template tutorial scripts/assets | Confirmed | asset scan |

## Assembly Boundaries

- No project-specific `.asmdef` or `.asmref` files found; runtime scripts compile into the default Assembly-CSharp assembly.

## Scenes And Startup Flow

- Build scenes: `Assets/Astral.unity` enabled at index 0; `Assets/Scenes/SampleScene.unity` retained but disabled.
- Likely startup scene: `Assets/Astral.unity`.
- Scene loading flow: no custom scene-loading system identified.

## Architecture

| Pattern | Finding | Confidence | Evidence |
| --- | --- | --- | --- |
| Runtime components | MonoBehaviour composition is the current approach | Likely | no first-party gameplay architecture existed before this feature |
| Input | Serialized movement/camera action maps plus a controller-owned runtime command map with persistent overrides | Confirmed | `Assets/InputSystem_Actions.inputactions`, `AstralCommandInput.cs`, player/camera scripts |
| Audio | One cached procedural 2D feedback source; no AudioMixer asset currently exists | Confirmed | `AstralFeedbackAudio.cs`, live mixer/source inventory |
| Objective flow | Component-owned runtime state with a separate uGUI presenter | Confirmed | `CrashedBeaconObjective.cs`, `BeaconObjectiveUI.cs` |
| Networking | No networking package or code identified | Confirmed | package and asset inspection |

## Coding Conventions

- Namespace style: new runtime code uses `AstralWilds`.
- Serialized fields: private fields with `[SerializeField]`.
- Async: none identified.
- Comments/docs: focused headers and intent comments only where useful.

## Testing And Validation

- EditMode tests: none found.
- PlayMode tests: none found.
- Current validation: Unity MCP compilation and scene inspection; runtime Play Mode validation remains to be performed.

## Available Unity Tooling

| Capability | Status | Evidence |
| --- | --- | --- |
| `unity.connection.status` | available | Unity MCP commands execute successfully |
| `unity.editor.version` | available | Unity MCP/local project version read |
| `unity.console.read` | available | `Unity_GetConsoleLogs` |
| `unity.scene.inspect` | available | `Unity_RunCommand` scene traversal |
| `unity.buildsettings.read` | available | `Unity_RunCommand` |
| `unity.gameobject.inspect` | available | `Unity_RunCommand` |
| `unity.asset.search` | available | `AssetDatabase` through `Unity_RunCommand` |
| `unity.tests.list` | unavailable/unverified | no test assets found |
| `unity.tests.run` | unavailable/unverified | no test assets found |
| `unity.playmode.read` | available/unverified | Editor is connected; Play Mode not yet entered |

## Important Constraints

- Preserve the existing input action asset and scene/build configuration unless a feature directly requires changes.
- Treat scene and project settings as high-impact serialized assets; inspect after every mutation.
- There is no established gameplay controller, camera rig, player prefab, or world content yet.

## Unknowns And Confidence

- The game’s final player model, animation system, resource systems, and hostile-creature systems are not yet present; the first beacon objective now exists as a placeholder implementation.
- Runtime objective UI uses uGUI legacy `Text` with Unity 6’s `LegacyRuntime.ttf`; no Meshy, Blender, or external art assets are used.
- Runtime camera feel and collision behavior require Play Mode validation with a playable environment.

## Source Files Inspected

- `ProjectSettings/ProjectVersion.txt`
- `Packages/manifest.json`
- `Assets/InputSystem_Actions.inputactions`
- `Assets/Astral.unity`
- `ProjectSettings/EditorBuildSettings.asset`
- `ProjectSettings/ProjectSettings.asset`
- `Assets/Settings/`

<!-- unity-onboarding:generated:end -->
