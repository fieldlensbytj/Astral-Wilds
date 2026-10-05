# Next steps - read this first

For a Claude Code session running in PowerShell with real shell access to this machine (unlike the Cowork scheduled-task bridge, which has none - see this repo's commit messages and `../../Astral Wilds/Docs/AI/CoworkReview-2026-10-05.md` for why). This file is kept up to date in place; it is not a log.

Full project context, conventions, and the dated history of how this repo got here live in the *other* repo at `C:\Users\camer\Astral Wilds\` - read `AGENTS.md` there first if you haven't already this session, then `Docs\AI\WorkQueueUnreal.md` (this repo's full history is documented there, including a "PICK UP HERE" block that mirrors this file).

## Current state (last updated 2026-10-05 by the Cowork daily session)

Local `master` is ahead of `origin/astral-wilds-unreal` - run `git log --oneline origin/astral-wilds-unreal..master` for the exact current list (this file's own commits add to the count each time it's updated, so no fixed number is given here). None of today's commits are build-verified yet - today's Cowork session had no compiler/PIE access at all. The substantive ones, oldest to newest:

- `7d83536` - adds `.gitattributes` (marks `*.uasset`/`*.umap` binary). Zero risk, not gameplay code.
- `fa1b858` - Resonance Weave first-pulse grace period (no flee on a weave's very first pulse, win or lose) + tightened interact reach (~6.7m -> ~3.7m). Updates 2 existing tests' expected result to match.
- `1c6f957` - Sigil debug-HUD draw position moved off dead-center + new test `FAstralResonanceWeave_Tick_FleeResumesAfterGracePeriod` (fills a TODO `fa1b858` left).
- Everything after `1c6f957` is this file itself being added/corrected - docs only, zero build risk.

Full reasoning for each, including the exact numbers and why, is in the commit messages (`git log -3 -p`).

## Do this, in order

```powershell
git pull   # in case anything else landed since
git log --oneline -5   # confirm you see 7d83536 / fa1b858 / 1c6f957 at the top
```

Build (engine is 5.7 - see `Astral_Wilds.uproject`'s `EngineAssociation`; confirm the install path below is still right before running it, it's inferred from a sibling UE_5.8 path a 2026-09-18 session logged, not re-verified today):

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" Astral_WildsEditor Win64 Development -project="C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject"
```

Then run the suite headless (should be **54/54** - 53 going into today, +1 new):

```powershell
& "D:\Games\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Users\camer\Astral_Wilds_Unreal\Astral_Wilds\Astral_Wilds.uproject" -ExecCmds="Automation RunTests AstralWilds; Quit" -unattended -nopause -nosplash
```

If both are green: `git push origin master:astral-wilds-unreal`, then update this file to say so (commits pushed, suite green, date) and mirror the same update into `Docs\AI\WorkQueueUnreal.md`'s "PICK UP HERE" block in the other repo.

If the build or a test fails: almost certainly `fa1b858` or `1c6f957` (both touch `AstralResonanceWeaveComponent`/its tests), not `7d83536` (docs/config only). `git revert <hash>` the specific bad one - each was written to be independently revertable. If it's `FAstralResonanceWeave_Tick_FleeResumesAfterGracePeriod` failing on a `PulseCallCount` assertion specifically, that commit's message says which stage's tick count to re-derive.

## After that's confirmed, still open

- A `NavMeshBoundsVolume` for `Lvl_ThirdPerson` so wildlife AI can actually path - needs the editor open, a Cowork session can't do this blind.
- The `Content/` Git LFS decision (238MB+ and growing as plain binaries) - TJ's call.
- A real human PIE playtest for game feel now that today's tuning has landed.

See `../../Astral Wilds/Docs/AI/WorkQueueUnreal.md` for the full dated history and design findings behind all of the above.
