# Server

Authoritative runtime entrypoint and boundary notes.

Responsibilities:
- session lifecycle
- authoritative state
- command validation
- tick-based transitions
- snapshot publishing
- event logging / AAR generation
- resilience handling

Entrypoint:
- `server/src/main.cpp`

Current behavior:
- loads runtime config from the example config set
- runs the authoritative reference session
- writes AAR artifacts to `assets/sample-aar/`
- writes runtime logs to `logs/session.log`
- writes a viewer-oriented sample output to `examples/sample-output.md`
