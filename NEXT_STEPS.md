# Next steps - read this first

For a Claude Code session running in PowerShell with real shell access to this machine (unlike the Cowork scheduled-task bridge, which has none - see this repo's commit messages and `../../Astral Wilds/Docs/AI/CoworkReview-2026-10-05.md` for why). This file is kept up to date in place; it is not a log.

Full project context, conventions, and the dated history of how this repo got here live in the *other* repo at `C:\Users\camer\Astral Wilds\` - read `AGENTS.md` there first if you haven't already this session, then `Docs\AI\WorkQueueUnreal.md`.

## Current state (last updated 2026-10-05, after the Glacielle rig `a70cfe9`)

Everything is **build-verified, tested and pushed** to `origin/astral-wilds-unreal`. Suite: **61/61**, 0 unloadable assets. Rendered bot playtest: the bot bonds with the (rigged) Glacielle.

**The Astrals in the world are the six Meshy models** (`43def68`): Cindrel (Ember, Skittish), Mossling (Verdant, Docile), Ironbur (Terra, Territorial), Ripplefin (Tide, Skittish), Stormrook (Volt, Aggressive), Glacielle (Frost, Skittish). Each has `Content/Astral/Species/<Name>/` with the imported GLB and `BP_Species_<Name>`. All use `DisplayYawOffset` -90 (the GLBs face +Y). Check new models with the dev command `Astral.LineupTest`, which writes `Saved/AutoPlaytest/lineup_<Name>.png`.

**Rigging** is scripted in headless Blender. The scripts are in the art repo at `C:\Users\camer\Astral Wilds\ArtSource\Blender\Scripts\<name>_rig_*.py`, and the rigged .blend/.fbx files are in `ArtSource\Blender\Rigs\`.
- **Glacielle is rigged** (`a70cfe9`): a 22-bone quadruped with Idle/Walk/Run, and a skeletal mesh decimated to 20k tris. Skin weights come from distance to bone segments, because heat weighting fails on its shard-covered surface. Material_0 is re-parented onto the static model's glTF material (full 4K PBR). The bondable test Astral is a Glacielle.
- **Cindrel is back on its static model** (TJ: its curved paw needs hand animation). The pilot's `Rigged/` assets (`db19d63`) are kept as a possible starting point.
- Engine side: `UAstralSpeciesData` has IdleAnim/WalkAnim/RunAnim plus authored speeds. `AAstralCharacter` plays them by ground speed (AnimationSingleNode, no Anim BP). `ClearRiggedDisplay()` (CallInEditor) drops back to the static display.
- Unrigged Astrals get procedural breathing, gait bob and turn lean (`68098cf`, `bProceduralMotion`).

**The navmesh on `Lvl_ThirdPerson` is fixed and committed** (`304a9f2`). A hand-placed `NavMeshBoundsVolume` (Location 0,0,150 / Scale 22,22,4.5) covers the platform top and all four ramps, and wild Astrals move on it via the native AI. Re-check it by eye (`P` overlay) if the level geometry changes.

**Open decision for TJ:** keep the scripted rigging approach or rig by hand, judged on how Glacielle looks and moves.

## How to verify (PowerShell)

Build (UE 5.7 at `D:\Games\Epic Games\UE_5.7`, verified 2026-10-05):

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" Astral_WildsEditor Win64 Development "-Project=C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject" -WaitMutex
```

Tests, headless. `-stdout` is needed to see the results; the GameFeatures "ensure" at startup is harmless, and its exit code 255 doesn't mean a test failed. Count the `Result={Success}` lines instead:

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject" -ExecCmds="Automation RunTests AstralWilds;Quit" -unattended -nopause -nosplash -NullRHI -log -stdout
```

Rendered bot playtest. It opens a game window for about 30s, then logs `[AutoPlaytest] RESULT:` to `Saved\Logs\Astral_Wilds.log` and saves screenshots to `Saved\AutoPlaytest\`:

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject" /Game/ThirdPerson/Lvl_ThirdPerson -game -windowed -ResX=1280 -ResY=720 "-ExecCmds=Astral.AutoPlaytest"
```

## Still open

- **Rig more Astrals** with the Glacielle pipeline where the body shape allows. Mossling is next, then possibly Ironbur and Stormrook. Ripplefin (a fish) needs a different rig, and Cindrel waits for hand animation.
- **Rig rough edges:** walk/run speeds are estimates, so some feet slide.
- **Mesh weight:** the static Meshy models are ~100-135MB each and due for retopology or decimation.
- **Wildlife AI: native C++ behaviour in place** (`AAstralWildlifeController::bUseNativeBehavior`, on by default). Docile wander, Skittish flee within 6m, Aggressive chase (slower than the player, gives up at 12m), Territorial guard home; Receptive Astrals always hold still. A designer StateTree can replace it later: author it, assign it to `StateTreeAI`, and set `bUseNativeBehavior` false.
- **Git LFS: done** (`32bd598`, forward-only). `*.uasset`/`*.umap` are LFS-tracked. Any clone or pull needs Git LFS installed, otherwise assets check out as text pointers and Unreal can't load them. The Meshy plugin is installed engine-wide, not in the project, so other machines need it too (`b06f5f7`).
- A human PIE playtest for game feel (tracking speed, pulse rhythm, how the rigs read). The bot only proves the loop works.
- Both first-pulse protections are now active: starting Stability, and no flee on the first pulse. If bonding feels too forgiving, `fa1b858`'s grace is the one to revisit.
