# Reviewer Brief

## What This Repository Is

Interceptor Command Simulation System is a C++ real-time simulation/control system built around:
- server-authoritative state
- TCP control + UDP state propagation
- replayable event history / post-engagement review
- resilience visibility under reconnect, timeout, and packet-loss conditions

## Suggested Review Order

1. `README.md`
2. `docs/architecture.md`
3. `docs/protocol.md`
4. `docs/operations.md`
5. `docs/aar.md`
6. `docs/test-report.md`

## Fast Path

If you only have a few minutes:
1. run `./build/icss_server --backend in_process`
2. open `assets/sample-aar/session-summary.md`
3. open `examples/sample-output.md`
4. compare `assets/sample-aar/unguided_intercept/session-summary.md`
5. compare `examples/sample-output-unguided_intercept.md`
6. skim `docs/protocol.md` and `docs/test-report.md`

## Five-Minute Review Path

- startup behavior: `./build/icss_server --backend in_process`
- live startup behavior: `./build/icss_server --backend socket_live --tick-limit 2`
- artifact summary: `assets/sample-aar/session-summary.md`
- viewer snapshot: `examples/sample-output.md`
- unguided_intercept comparison summary: `assets/sample-aar/unguided_intercept/session-summary.md`
- unguided_intercept comparison output: `examples/sample-output-unguided_intercept.md`
- verification surface: `docs/test-report.md`

## What To Look For

### 1. Authority Boundary
- the server owns validation and assessment
- clients issue requests or render state
- post-engagement review is derived from server-side history

### 2. Transport Split
- TCP is used for control, acknowledgements, and post-engagement review requests
- UDP is used for snapshots, telemetry, and viewer liveness

### 3. Operability
- structured runtime log under `logs/session.log`
- generated post-engagement review artifacts under `assets/sample-aar/`
- generated viewer-oriented sample output under `examples/sample-output.md`
- unguided_intercept comparison artifacts under `assets/sample-aar/unguided_intercept/` and `examples/sample-output-unguided_intercept.md`

### 4. Resilience
- reconnect/reacquire visibility
- timeout visibility
- degraded picture status under packet loss
- snapshot batching/filtering behavior

## Evidence Map

### Runtime / transport
- server modes: `server/src/main.cpp`
- transport backend: `common/src/transport.cpp`, `common/src/transport_common.cpp`, `common/src/transport_inprocess.cpp`, `common/src/transport_socket_live.cpp`
- protocol and framing: `common/include/icss/protocol/`, `common/src/serialization.cpp`, `common/src/frame_codec.cpp`

### Viewer / observability
- renderer: `common/src/ascii_tactical_view.cpp`
- tactical display entrypoint: `clients/tactical-viewer/src/main.cpp`

### Post-Engagement Review / output artifacts
- replay timeline: `assets/sample-aar/replay-timeline.json`
- session summary: `assets/sample-aar/session-summary.md`
- machine-readable summary: `assets/sample-aar/session-summary.json`
- sample output: `examples/sample-output.md`
- unguided_intercept replay timeline: `assets/sample-aar/unguided_intercept/replay-timeline.json`
- unguided_intercept session summary: `assets/sample-aar/unguided_intercept/session-summary.md`
- unguided_intercept sample output: `examples/sample-output-unguided_intercept.md`

### Verification
- test index: `docs/test-report.md`
- process-level live smoke: `tests/protocol/src/server_process_live_smoke.cpp`
- process-level JSON smoke: `tests/protocol/src/server_process_live_json_smoke.cpp`

## Commands

### Configure
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
```

### Build
```bash
cmake --build build
```

### Test
```bash
ctest --test-dir build --output-on-failure
```

### Server
```bash
./build/icss_server --backend in_process
./build/icss_server --backend socket_live --tick-limit 2
```

### Useful targeted checks
```bash
ctest --test-dir build --output-on-failure -R 'server_process_live_smoke|server_process_live_json_smoke'
ctest --test-dir build --output-on-failure -R 'runtime_config_invalid_values_smoke|server_invalid_.*_cli'
```

## Useful Outputs

- Session summary: `assets/sample-aar/session-summary.md`
- Replay timeline: `assets/sample-aar/replay-timeline.json`
- Sample viewer output: `examples/sample-output.md`
- UnguidedIntercept session summary: `assets/sample-aar/unguided_intercept/session-summary.md`
- UnguidedIntercept sample viewer output: `examples/sample-output-unguided_intercept.md`
- Runtime log: `logs/session.log`

## Current Verification Snapshot

The current regression set covers:
- protocol and payload serialization
- JSON and binary framing
- transport backend behavior
- process-level live server smoke for binary and JSON framing
- runtime config validation
- post-engagement review control semantics
- resilience and viewer picture-status transitions
