# Test Report

## Goal

Verify that the system matches its intended transport, runtime, and resilience boundaries.

## Canonical Commands

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

## Coverage

Current verification covers:
- the CMake project configures successfully
- the primary executables build successfully
- the shared protocol schema compiles and passes smoke checks
- concrete payload serialization/parsing round-trips successfully
- JSON and binary frame codecs round-trip successfully
- transport backend dispatch behavior is verified
- live transport session policy is verified
- AAR control semantics are verified
- the scenario flow passes end-to-end regression
- invalid command ordering is rejected and logged
- runtime config loading is verified against the example config set
- runtime artifact paths are verified against configurable repo roots
- runtime session logging is verified against generated structured log output
- replay cursor stepping is verified against viewer output
- resilience/replay rendering behavior passes smoke verification
- live viewer heartbeat timeout behavior is verified against the socket backend
- live viewer freshness transitions are verified against the socket backend
- live snapshot batching/filtering is verified against the socket backend

## Latest Result

- configure: passed
- build: passed
- test: passed (`15/15` tests)
- runtime smoke: passed (`icss_server`, `icss_command_console`, `icss_tactical_viewer`)
- verified targets:
  - `protocol_smoke`
  - `payload_codec_smoke`
  - `frame_codec_smoke`
  - `transport_backend_smoke`
  - `socket_live_single_session_policy_smoke`
  - `scenario_flow`
  - `validation_rejects_invalid_flow`
  - `runtime_config_smoke`
  - `runtime_artifact_paths_smoke`
  - `session_log_smoke`
  - `replay_cursor_smoke`
  - `resilience_smoke`
  - `timeout_smoke`
  - `socket_live_viewer_timeout_smoke`
  - `socket_live_snapshot_batching_smoke`

## Acceptance Checks

### 1. Scenario Completeness
- [ ] scenario runs from start to AAR
- [ ] command validation occurs on server
- [ ] final judgment is produced by server

### 2. Multi-Client Coverage
- [ ] command console participates
- [ ] tactical viewer receives state/telemetry updates
- [ ] roles are meaningfully separated

### 3. Protocol Coverage
- [ ] TCP responsibilities exercised
- [ ] UDP snapshot/telemetry responsibilities exercised
- [ ] transport split documented clearly

### 4. Resilience Coverage
- [ ] at least one abnormal case is exercised
- [ ] logs show the abnormal case clearly
- [ ] system behavior after the abnormal case is understandable

### 5. AAR Coverage
- [ ] one sample session can be replayed/reconstructed
- [ ] event timeline includes key command/judgment points

### 6. Operational Coverage
- [ ] config separation exists
- [ ] logs have meaningful structure
- [ ] session cleanup/end-state is visible

## Related Outputs

- sample AAR: `assets/sample-aar/session-summary.md`
- logs: pending
- screenshots: pending
- protocol doc: `docs/protocol.md`
- concrete build/test commands: see canonical commands above
