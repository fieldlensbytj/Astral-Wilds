# Astral Wilds Work Queue

Checkpoint: 2026-09-16 07:39:48 Asia/Qatar (UTC+3). Deadline: stabilize/verify at 12:30; handoff at 13:00.

## Current priority

1. Exercise the full keyboard sequence in Play Mode and capture observed results.
2. Exercise the full keyboard sequence in Play Mode and capture observed results.
3. Add only fixes demonstrated by that run.
4. Verify persistent save/load across an actual Play Mode exit/re-entry.

## Status

- Domain party/2v2 rules: Verified by Unity MCP smoke test.
- Reserve switching and duplicate-action guard: Verified by Unity MCP smoke test.
- Playable runtime controller: Implemented and compiled; attached to `Assets/Astral.unity` and scene saved. End-to-end input remains unverified.
- Full end-to-end gameplay: Blocked pending Unity Editor connection and input-capable Play Mode verification.
- Meshy/paid services: no spend authorized or performed in this work window.
- Claude direct diagnostic: READY, exit code 0. The one follow-up JSON review returned empty output with no findings/error payload; it is not treated as a passed review.
- Unity MCP reconnect checkpoint: controller attached, Play Mode entered/exited, Console has 0 errors and 1 render-texture warning from capture.
