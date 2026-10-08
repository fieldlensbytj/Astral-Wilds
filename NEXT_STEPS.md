# Next steps - read this first

For a Claude Code session running in PowerShell with real shell access to this machine (unlike the Cowork scheduled-task bridge, which has none - see this repo's commit messages and `../../Astral Wilds/Docs/AI/CoworkReview-2026-10-05.md` for why). This file is kept up to date in place; it is not a log.

Full project context, conventions, and the dated history of how this repo got here live in the *other* repo at `C:\Users\camer\Astral Wilds\` - read `AGENTS.md` there first if you haven't already this session, then `Docs\AI\WorkQueueUnreal.md`.

## Current state (last updated 2026-10-08, after rigging Ironbur)

Everything is **build-verified, tested and pushed** to `origin/astral-wilds-unreal`. Suite: **61/61**. Rendered bot playtest: PASS, bonded with the rigged Glacielle in 7.2s.

**Latest (2026-10-08):** TJ loves Ironbur ("the best so far") and asked for it to travel further, so Territorial Astrals now patrol (see Wildlife AI). TJ then said Glacielle doesn't feel as natural, specifically **stiff/robotic legs** and **starts, stops and turns**. Fixes, **waiting for TJ's verdict**:
- Legs: Glacielle's front legs are modelled dead straight, and her strides ran to 98.5% of reach, so they locked straight (5 deg bend) in stance. `glacielle_rig_gait_style.py` gives her a 4cm crouch, strides within 92% of reach, and 1.6x foot lift. The front legs now always keep 31 deg+ of bend. These are opt-in armature properties in `quadruped_reanimate.py` (`crouch`, `reach_margin`, `lift_scale`), so Ironbur and Mossling are unchanged.
- Starts and turns: wander targets prefer points within ~75 deg of facing, so Astrals set off forward and curve instead of spinning on the spot.
- Stops: a flee that ends eases into a walk and carries on `FleeRunOut` (4m) before pausing, instead of braking 450 -> 0 in a second.
- The bot playtest walks round Astrals that box the Mage in (it used to fail when Ironbur and Stormrook both chased the Mage at spawn): 6/6 PASS.
Earlier: TJ approved the trot Run on Glacielle + Mossling on 2026-10-06 (art repo `92c4081`, this repo `8de7430`); a gallop was rejected (see the Rigging bullets). Next pick: Stormrook (consider giving it a gait style from the start), or more tuning from TJ's feedback.

**The Astrals in the world are the six Meshy models** (`43def68`): Cindrel (Ember, Skittish), Mossling (Verdant, Docile), Ironbur (Terra, Territorial), Ripplefin (Tide, Skittish), Stormrook (Volt, Aggressive), Glacielle (Frost, Skittish). Each has `Content/Astral/Species/<Name>/` with the imported GLB and `BP_Species_<Name>`. All use `DisplayYawOffset` -90 (the GLBs face +Y). Check new models with the dev command `Astral.LineupTest`, which writes `Saved/AutoPlaytest/lineup_<Name>.png`.

**Rigging** is scripted in headless Blender (5.2 at `C:\Program Files\Blender Foundation\Blender 5.2\blender.exe -b --python <script>`). UE imports go through `UnrealEditor-Cmd -run=pythonscript`; Interchange crashes with a Slate assert after the import, but the assets are already saved. Close the editor first. The scripts are in the art repo at `C:\Users\camer\Astral Wilds\ArtSource\Blender\Scripts\<name>_rig_*.py`, and the rigged .blend/.fbx files are in `ArtSource\Blender\Rigs\`.
- **Glacielle is rigged** (`a70cfe9`): a 22-bone quadruped with Idle/Walk/Run, and a skeletal mesh decimated to 20k tris. Skin weights come from distance to bone segments, because heat weighting fails on its shard-covered surface. Material_0 is re-parented onto the static model's glTF material (full 4K PBR). The bondable test Astral is a Glacielle. Gait style since 2026-10-08: `glacielle_rig_gait_style.py` (run it before reanimating).
- **Mossling is rigged** the same way (art repo `feec1bd`). Everything above z=1.05 (antlers and the leaf crest sweeping back over the spine) is pinned to the head bone. The prep script's automatic heading fails on it (it picks the two antlers as head and tail), so the facing was set by hand with `mossling_rig_rotate.py -- -126`.
- **Ironbur is rigged** (2026-10-08, awaiting TJ's review). Script: `ironbur_rig_rig_ironbur.py`, with landmarks from `ironbur_rig_columns.py` (median leg columns traced up from each paw). Auto-heading worked. It is a low, 2.8m-long boar that leans onto big clawed fore paws ~0.36m ahead of its shoulders, and it was modelled mid-stride with the right fore paw lifted under the jaw. Two armature properties handle that in `quadruped_reanimate.py`: `plant_fr` (where that paw stands, mirrored from the left) and `fit_stance` (pull the fore stance back until the stride fits the legs' reach; opt-in, so Glacielle/Mossling are unchanged). The head pin only takes verts nearer the head than a fore leg; otherwise the lifted paw's top stays with the head and smears. Skinning is Glacielle's refit recipe (4th power / top 4, per-piece leg sides). Speeds: walk 137, run 351 cm/s. Film it with `Astral.MotionCapture Ironbur 20 Alone` (the third argument is either `Alone`, which parks the hidden Mage 50m away so a Territorial Astral patrols instead of chasing it, or an archetype override such as `Docile`).
- **Cindrel is back on its static model** (TJ: its curved paw needs hand animation). The pilot's `Rigged/` assets (`db19d63`) are kept as a possible starting point.
- Engine side: `UAstralSpeciesData` has IdleAnim/WalkAnim/RunAnim plus authored speeds. `UAstralLocomotionAnimInstance` (native, no Anim BP) crossfades them by ground speed like a 1D blend space: eased weights, one shared gait phase for Walk/Run, cadence from ground speed / authored speed. Rigged Astrals also get the turn lean. `ClearRiggedDisplay()` (CallInEditor) drops back to the static display.
- **Clips are generated by `ArtSource/Blender/Scripts/quadruped_reanimate.py -- <Name> <outdir>`** (run on `Rigs/<Name>_Rigged.blend`, rewrites it and the .fbx). It drives the feet with IK along planted stance and smooth swing paths, with overlapping body/head/tail motion, at 60fps. It writes the Walk/Run ground speeds to `speeds.txt`, and those set `WalkAnimSpeed`/`RunAnimSpeed` (x100 x DisplayHeight/190). To update UE (close the editor first): run `Tools/RigImport/reimport_rig.py` once per Astral. It deletes `Rigged/` and imports fresh, because a reimport over existing assets crashes before the animations save. Then run `Tools/RigImport/wire_rigs.py`, which re-parents `Material_0` and sets the species BP clips and speeds from `speeds.txt`. Usage is in each script's header. Both exit non-zero from a harmless Slate assert; check the assets and `wire_rigs.txt` instead.
- **The Run is a fast trot (TJ, 2026-10-06).** Diagonal pairs alternate: bl+fr, then br+fl. A gallop was tried first. TJ rejected it because the near-synchronous hind pair read as static back legs. The hind legs get their own stroke, sized per rig to the hip-to-hock reach: about 0.8m against 0.5m for the fronts. It reaches under the belly, pushes off behind, and folds the hock in the swing. The hind stance duty is scaled so the ground speed is unchanged. To review a gait, run `ArtSource/Blender/Scripts/gait_strip.py -- <Rig.blend> <Action> 12 <out.png> 2`, which renders a side-view strip with the leg bones drawn over the mesh (hind legs red, front legs green).
- Unrigged Astrals get procedural breathing, gait bob and turn lean (`68098cf`, `bProceduralMotion`).

**The navmesh on `Lvl_ThirdPerson` is fixed and committed** (`304a9f2`). A hand-placed `NavMeshBoundsVolume` (Location 0,0,150 / Scale 22,22,4.5) covers the platform top and all four ramps, and wild Astrals move on it via the native AI. Re-check it by eye (`P` overlay) if the level geometry changes.

**Open decision for TJ:** keep the scripted rigging approach or rig by hand, judged on how Glacielle and Mossling look and move.

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


Motion review: films one Astral wandering (or fleeing, for skittish ones near the Mage) with a side camera. It writes frames to `Saved\AutoPlaytest\motion\NNN.png` (15/s) and logs per-frame speed, yaw rate and blend weights (`[MotionCapture]` lines). To tile the frames into one contact sheet, run `blender -b --python "C:\Users\camer\Astral Wilds\ArtSource\Blender\Scripts\contact_sheet.py" -- <motion dir> <first> <count> <cols> <out.png> [crop] [step]`. **Gotcha:** the PNG numbers don't match the log's `f=` counter. To find which motion state a PNG shows, match it to its `[AutoPlaytest] screenshot .../NNN.png` line. Frames after a Flee -> Wander switch are often her standing still, not a bug.

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject" /Game/ThirdPerson/Lvl_ThirdPerson -game -windowed -ResX=960 -ResY=540 -benchmark -fps=30 "-ExecCmds=Astral.MotionCapture Mossling 10"   # optional 3rd arg: Alone (Mage parked far away) or an archetype override, e.g. Docile
```

## Still open

- **Rig more Astrals** with the Glacielle pipeline where the body shape allows (Glacielle, Mossling and Ironbur done). Stormrook is the next candidate. Ripplefin (a fish) needs a different rig, and Cindrel waits for hand animation.
- **Locomotion tuning (2026-10-05):** `WanderSpeed` 200 -> 140 (TJ to confirm; 200 read as scurrying on these short legs). Wild Astrals use acceleration-driven paths with a 1.5m braking zone, low braking friction (stops glide rather than snap), 240 deg/s turns, and a spine bend into turns from the anim instance. Judge changes with `Astral.MotionCapture` (see How to verify), not just the bot.
- **Mesh weight:** the static Meshy models are ~100-135MB each and due for retopology or decimation.
- **Wildlife AI: native C++ behaviour in place** (`AAstralWildlifeController::bUseNativeBehavior`, on by default). Docile wander, Skittish flee within 6m, Aggressive chase (slower than the player, gives up at 12m), Territorial patrols around home (wanders within RoamRadius 8m like Docile, 2026-10-08: it used to stand still) and chases a player inside its 5m territory, then walks straight home; Receptive Astrals always hold still. A designer StateTree can replace it later: author it, assign it to `StateTreeAI`, and set `bUseNativeBehavior` false.
- **Git LFS: done** (`32bd598`, forward-only). `*.uasset`/`*.umap` are LFS-tracked. Any clone or pull needs Git LFS installed, otherwise assets check out as text pointers and Unreal can't load them. The Meshy plugin is installed engine-wide, not in the project, so other machines need it too (`b06f5f7`).
- A human PIE playtest for game feel (tracking speed, pulse rhythm, how the rigs read). The bot only proves the loop works.
- Both first-pulse protections are now active: starting Stability, and no flee on the first pulse. If bonding feels too forgiving, `fa1b858`'s grace is the one to revisit.
