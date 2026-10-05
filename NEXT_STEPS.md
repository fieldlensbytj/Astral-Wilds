# Next steps - read this first

For a Claude Code session running in PowerShell with real shell access to this machine (unlike the Cowork scheduled-task bridge, which has none - see this repo's commit messages and `../../Astral Wilds/Docs/AI/CoworkReview-2026-10-05.md` for why). This file is kept up to date in place; it is not a log.

Full project context, conventions, and the dated history of how this repo got here live in the *other* repo at `C:\Users\camer\Astral Wilds\` - read `AGENTS.md` there first if you haven't already this session, then `Docs\AI\WorkQueueUnreal.md`.

## Current state (last updated 2026-10-05, navmesh fixed by hand in-editor via TJ + Cowork walkthrough)

Everything is **build-verified, tested and pushed** to `origin/astral-wilds-unreal`. Suite: **61/61**. Rendered bot playtest: **4/4 bonded** (7.2-9.0s).

**Navmesh on `Lvl_ThirdPerson` is fixed.** Previous attempts (see commit history / below) only generated navmesh inside the central platform, never on its walkable top. TJ placed a fresh `NavMeshBoundsVolume` by hand (Location 0,0,150 / Scale 22,22,4.5 - covers the full arena), ran Build > Build Paths, and confirmed via the `P` nav-display overlay that the ENTIRE top of the central platform and all four ramps show green, not just the floor. Saved (External Actors under `Content/__ExternalActors__/ThirdPerson/Lvl_ThirdPerson/` - still needs `git add` + commit, not done yet as of this note). Wildlife still won't move on it, though - see Still Open below, that's a separate StateTree problem.

Latest substantive commits (`git log --oneline -10` for the rest):

- `a9b7fcf` - Resonance Weave starts at 20 Stability (`StartingStability`, capped at half `RequiredStability`), TJ's chosen fix for the first-pulse cliff. Also fixes the tick count in `FleeResumesAfterFirstPulseGracePeriod`, which failed on its first real run.
- `1c6f957` / `fa1b858` - first-pulse no-flee grace (kept alongside starting Stability), tighter interact reach (~3.7m, confirmed on screen), Sigil moved up off the Mage. Verified by the session above.

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

- **Wildlife AI: native C++ behaviour in place** (`AAstralWildlifeController::bUseNativeBehavior`, on by default). Docile wander, Skittish flee within 6m, Aggressive chase (slower than the player, gives up at 12m), Territorial guard home; Receptive Astrals always hold still. Verified in a rendered run. A designer StateTree can replace it later: author it, assign it to `StateTreeAI`, and set `bUseNativeBehavior` false.
- ~~Navmesh: tried and backed out.~~ **Fixed 2026-10-05** - see Current State above. Manually placing the volume and running Build Paths worked where the earlier dynamic-generation attempt didn't; root cause of the original failure still isn't understood, just worked around. The spawner already waits for navmesh when navigation data exists and falls back to ground traces otherwise - worth confirming wild Astrals actually use the new navmesh once a StateTree exists for them to run.
- **Git LFS: done** (`32bd598`, forward-only). `*.uasset`/`*.umap` are LFS-tracked, and all 759 assets are uploaded. Any clone or pull needs Git LFS installed, otherwise assets check out as text pointers and Unreal can't load them. History before `32bd598` is unchanged.
- A human PIE playtest for game feel (tracking speed, pulse rhythm). The bot only proves the loop works.
- Both first-pulse protections are now active: starting Stability, and no flee on the first pulse. If bonding feels too forgiving, `fa1b858`'s grace is the one to revisit.
