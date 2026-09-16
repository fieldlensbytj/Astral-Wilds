# Astral Wilds Verification

## 2026-09-16 checkpoint

### Verified by executed Unity MCP domain smoke test

- Party count reached 6.
- Seventh Astral reached reserve.
- Two active player and two active opponent slots produced 4 total active Astrals.
- Third activation was rejected on both sides.
- Target action queue accepted a valid target scope.
- Fainted active slot was replaced without changing the other active slot.
- All opponent Astrals fainting ended the battle state.
- Duplicate reward was rejected.
- JSON campaign round-trip preserved a six-member party.
- Reserve replacement removed one reserve entry.
- Duplicate action queueing on one active slot was rejected.

### Current unverified or blocked

- Unity MCP later reconnected; `AstralDemoLoopController.cs` compiled successfully, was attached to `Assets/Astral.unity`, and the scene was saved.
- Exploration encounter, interactive battle UI, opponent turns, recruitment UI, party management, persistent file save/load, second encounter, and return-to-exploration have not been verified through gameplay input.
- No built executable test was performed.
- Play Mode was entered and exited, but keyboard gameplay input could not be injected through the available MCP surface.

### Tool evidence

- Claude executable version: `2.1.268`.
- Corrected Claude diagnostic: direct PowerShell call returned `READY`, exit code 0.
- The single focused follow-up JSON review returned empty output and no findings/error payload; it is not review evidence.
- Latest Console query: 0 errors, 1 warning (`Releasing render texture that is set to be RenderTexture.active!`) associated with camera capture.
