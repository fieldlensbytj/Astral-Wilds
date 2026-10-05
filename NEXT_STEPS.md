# Next steps - read this first

For a Claude Code session running in PowerShell with real shell access to this machine (unlike the Cowork scheduled-task bridge, which has none - see this repo's commit messages and `../../Astral Wilds/Docs/AI/CoworkReview-2026-10-05.md` for why). This file is kept up to date in place; it is not a log.

Full project context, conventions, and the dated history of how this repo got here live in the *other* repo at `C:\Users\camer\Astral Wilds\` - read `AGENTS.md` there first if you haven't already this session, then `Docs\AI\WorkQueueUnreal.md`.

## Current state (last updated 2026-10-05 by a Claude Code session with shell access)

Everything is **build-verified, tested and pushed** to `origin/astral-wilds-unreal`. Suite: **56/56**. Rendered bot playtest: **4/4 bonded** (7.2-9.0s).

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

- A `NavMeshBoundsVolume` for `Lvl_ThirdPerson` so wildlife AI can path. The spawner now falls back to ground traces, but wild Astrals can't move.
- The `Content/` Git LFS decision (238MB+ of plain binaries) - TJ's call.
- A human PIE playtest for game feel (tracking speed, pulse rhythm). The bot only proves the loop works.
- Both first-pulse protections are now active: starting Stability, and no flee on the first pulse. If bonding feels too forgiving, `fa1b858`'s grace is the one to revisit.
