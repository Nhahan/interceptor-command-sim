# Server Plan

Planned ownership for the authoritative runtime.

Expected responsibilities:
- session lifecycle
- authoritative state
- command validation
- tick-based transitions
- snapshot publishing
- event logging / AAR generation
- resilience handling

Committed entrypoint:
- `server/src/main.cpp`

Current baseline behavior:
- loads runtime config from the committed example config set
- runs the authoritative demo session
- writes AAR artifacts to `assets/sample-aar/`
- writes runtime logs to `logs/session.log`
- writes a viewer-oriented sample output to `examples/sample-output.md`
