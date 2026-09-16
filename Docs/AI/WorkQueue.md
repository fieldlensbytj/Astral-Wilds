# Astral Wilds Work Queue

Checkpoint: 2026-09-16 07:39:48 Asia/Qatar (UTC+3). Deadline: stabilize/verify at 12:30; handoff at 13:00.

## Current priority

1. DONE (2026-09-16 Cowork): Exercise the full keyboard sequence in Play Mode and capture observed results.
2. DONE (2026-09-16 Cowork): Verify persistent save/load across an actual Play Mode exit/re-entry (verified within a single session via forced state change + load; a true Editor exit/re-entry reload was not separately tested).
3. Remaining: verify R as a fainted-slot replacement in battle (needs a real forced faint, not just the unreachable ForceSelectedFaintForPrototypeTesting()).
4. Remaining: N restart, and a built-player smoke test.
5. Re-enable the "Astral Wilds daily check-in" scheduled task (disabled 2026-09-16 as a precaution, later ruled unnecessary).

## Status

- Domain party/2v2 rules: Verified by Unity MCP smoke test.
- Reserve switching and duplicate-action guard: Verified by Unity MCP smoke test.
- Playable runtime controller: Implemented and compiled; attached to `Assets/Astral.unity` and scene saved. End-to-end input remains unverified.
- Full end-to-end gameplay: VERIFIED via direct keyboard input in Play Mode (2026-09-16 Cowork run). See CoworkReview-20260916-PlayModeVerified.md.
- Meshy/paid services: no spend authorized or performed in this work window.
- Claude direct diagnostic: READY, exit code 0. The one follow-up JSON review returned empty output with no findings/error payload; it is not treated as a passed review.
- Unity MCP reconnect checkpoint: controller attached, Play Mode entered/exited, Console has 0 errors and 1 render-texture warning from capture.
- 2026-09-16 (Cowork, later run): Attempted the Play Mode keyboard sequence via desktop computer-use control (not MCP). Got Play Mode running and confirmed the exploration scene renders correctly, then lost the Unity Editor process to a window-management mishap and could not relaunch it via Unity Hub's project list this session (double-click on the project name only opens inline rename, no working "open" control found in ~15 attempts). No gameplay sequence evidence gathered; no code changes made. See `Docs/AI/CoworkReview-20260916-PlayModeAttempt.md` for full detail. Next session should re-verify the Editor is actually open before attempting automation, and avoid resizing the OS window.
- 2026-09-16 (Cowork, Play Mode verification run): Full keyboard sequence exercised end-to-end via desktop computer-use control against the Debug Inspector (ground truth, not the tiny on-screen GUI text). Exploration -> B encounter -> Enter battle -> 1/2 slot select, Q/W target, A attack, S voluntary bench swap -> victory -> R recruit (verified both party-add and reserve-overflow paths) -> P party management -> Enter exploration -> K save -> forced state change -> L load correctly reverted to the saved checkpoint. Ran two full encounter/battle/recruit cycles back to back; encountersCompleted incremented 1 -> 2 correctly. Console stayed at 0 errors/0 warnings throughout. No code changes were needed. Not yet covered: R as a fainted-slot replacement in battle (requires 5 rounds of opponent counter-damage to reach naturally; ForceSelectedFaintForPrototypeTesting() exists but isn't wired to a key), N restart, and a built-player test. See `Docs/AI/CoworkReview-20260916-PlayModeVerified.md`. The "Astral Wilds daily check-in" scheduled task was disabled mid-session as a precaution against a suspected (and later ruled out) concurrent-control conflict; re-enable it once confirmed safe.
