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
- supports `in_process` and `socket_live` server modes
- supports deterministic `--sample-mode tracked_intercept|unguided_intercept` generation for the in-process path
- writes AAR artifacts to `assets/sample-aar/`
- writes runtime logs to `logs/session.log`
- writes a viewer-oriented sample output to `examples/sample-output.md`
- auto-archives after assessment in the normal command flow so review starts from an archived run

Executable modes:
- `./build/icss_server --backend in_process`
- `./build/icss_server --backend socket_live --tick-limit <N>`
- `./build/icss_server --backend socket_live --tcp-port 0 --udp-port 0 --tick-limit <N> --tick-sleep-ms <N>`
- `./build/icss_server --backend socket_live --run-forever --tick-sleep-ms <N>`

Startup output:
- backend and bind information
- frame format
- heartbeat interval/timeout
- delivery policy for live snapshot transmission
- current viewer state and latest snapshot sequence
- command/viewer connection state and latest event type
- shutdown status when the process exits due to a signal
