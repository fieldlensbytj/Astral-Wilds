# Astral Wilds — Agent Instructions

This repository is worked on by multiple AI collaborators on the same local clone: OpenAI Codex, Claude Code, and a scheduled Cowork session, plus TJ directly. A GitHub mirror now exists at https://github.com/fieldlensbytj/Astral-Wilds (remote `origin`, branch `master`). Previously this repo was local-only; the remote was added 2026-09-17.

## ENGINE STATUS (as of 2026-09-18) — READ THIS FIRST

**Unreal Engine is now the active engine. Unity is deprecated/frozen.** TJ confirmed this directly on 2026-09-18.

- Active project: `Astral_Wilds_Unreal/Astral_Wilds/` (Unreal Engine 5.8, C++, Epic's Third Person template as a base). This is where new gameplay work goes.
- Frozen project: everything under `Assets/`, `ProjectSettings/`, `Astral Wilds.slnx`, etc. (the Unity project) at the repo root. Do not add new features here. It is left in place for now (not deleted/archived yet) — TJ has not asked for it to be removed, just no longer actively developed. If you need to reference validated game-design/balance decisions (battle math, economy rules, etc.), the Unity C# implementation is a working reference, but port intent to C++ rather than extending the C# further.
- The Unreal port began 2026-09-17/18 with `AstralCombatRules` (static Covenant-of-Two damage math) and `UAstralBattleEngine` (2v2 battle mechanics: queuing attacks/Arc Burst/Guard, opponent counterattacks, retargeting), ported from the validated Unity prototype logic in `AstralBattleEngine.cs`/`AstralDemoLoopController.cs`. `AstralMageCharacter` wires these into the template's Enhanced Input character.
- Old Unity-specific instructions below (WorkQueue priorities, Verification narratives, etc.) describe the frozen Unity project's history and are kept for reference/provenance, not as an active task list. Check `Docs/AI/WorkQueue.md`'s top-of-file status note (if present) for the current Unreal priority list before starting work.
- The non-negotiable no-real-money economy constraint (see `Docs/Design/EconomyPolicy.md`) applies equally to the Unreal build; port/re-implement that rule and its test coverage in C++ rather than assuming it's inherited for free.

## Git workflow (do this every session, automatically — no need to ask TJ)

1. Before starting any work, run `git pull` to sync with `origin/master`, in case another agent or TJ pushed since your last run.
2. Work normally: small, conservative, reversible changes; commit with clear messages describing what and why.
3. Before ending your session, if you made any commits, run `git push` to sync them back to `origin/master`.
4. If a push is rejected because the remote has commits you don't have, run `git pull` again to merge them in (resolve any conflicts if they occur), then push again. Never force-push over another agent's or TJ's work.

## Shared context (read at the start of every session)

- `Docs/AI/WorkQueue.md` — current priority list and dated status log.
- `Docs/AI/Verification.md` — detailed, dated verification narratives.
- `Docs/AI/AutonomousWorkReport.md` — prior autonomous session summaries.
- `Docs/AI/UnityProjectContext.md`, `Docs/AI/AstralFullTaskAudit.md` — additional project context.

After finishing a session, add a dated note under `Docs/AI/` (e.g. `Docs/AI/CoworkReview-<date>.md` or an entry in `WorkQueue.md`'s Status log) so the next agent — human or AI — has full context without re-deriving it.

## Project conventions

- Don't fix without a verified repro. Don't guess-fix or refactor speculatively.
- Keep changes small, conservative, and reversible — multiple agents touch this codebase concurrently.
- Non-negotiable economy constraint: no real-money purchases, paywalls, premium currency, paid progression, or rewarded-ad currency. All in-game currency is earned through gameplay. See `Docs/Design/EconomyPolicy.md`.
- Any paid third-party generation call (Meshy, etc.) requires an explicit per-call credit ceiling and should be flagged, per the project's documented spending ceiling.
