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
- the server CLI accepts valid backend modes and rejects invalid backend/frame selections
- invalid runtime/config values are rejected before execution
- server CLI overrides are validated and reflected in startup output
- CLI override precedence over file config is verified
- process-level live smoke verifies CLI override precedence against file config
- startup output exposes backend, bind, heartbeat, and delivery settings
- startup output exposes connection-state and latest-event summaries
- the executable `socket_live` server path is exercised through a process-level live smoke
- process-level live smoke covers AAR response and scenario stop handling
- process-level live smoke covers both binary and JSON TCP framing
- process-level live smoke verifies artifact/log generation for executable live mode
- long-running live mode handles signal-driven shutdown and flushes outputs
- startup output exposes current viewer state and idle/no-snapshot conditions
- the shared protocol schema compiles and passes smoke checks
- concrete payload serialization/parsing round-trips successfully
- JSON and binary frame codecs round-trip successfully
- transport backend dispatch behavior is verified
- live transport session policy is verified
- AAR control semantics are verified
- the scenario flow passes end-to-end regression
- invalid command ordering is rejected and logged
- duplicate start/stop and post-archive control requests are rejected and logged
- runtime config loading is verified against the example config set
- runtime artifact paths are verified against configurable repo roots
- runtime artifacts and logs expose stable schema/version metadata
- baseline artifact generation is deterministic across repeated runs
- session summary is emitted in both Markdown and JSON forms
- runtime session logging is verified against generated structured log output
- replay cursor stepping is verified against viewer output
- resilience/replay rendering behavior passes smoke verification
- live viewer heartbeat timeout behavior is verified against the socket backend
- live viewer freshness transitions are verified against the socket backend
- degraded freshness under packet loss is verified in rendered output
- live snapshot batching/filtering is verified against the socket backend

## Latest Result

- configure: passed
- build: passed
- test: passed (`33/33` tests)
- runtime smoke: passed (`icss_server`, `icss_command_console`, `icss_tactical_viewer`)
- cli smoke: passed (`server_inprocess_cli_smoke`, `server_socket_live_cli_smoke`)
- idle cli smoke: passed (`server_socket_live_idle_cli_smoke`)
- process live smoke: passed (`server_process_live_smoke`, `server_process_live_json_smoke`, `server_process_live_run_forever_smoke`)
- verified targets:
  - `protocol_smoke`
  - `payload_codec_smoke`
  - `frame_codec_smoke`
  - `transport_backend_smoke`
  - `socket_live_single_session_policy_smoke`
  - `scenario_flow`
  - `validation_rejects_invalid_flow`
  - `runtime_config_smoke`
  - `runtime_config_invalid_values_smoke`
  - `server_cli_override_precedence_smoke`
  - `runtime_artifact_paths_smoke`
  - `runtime_artifact_determinism_smoke`
  - `session_log_smoke`
  - `replay_cursor_smoke`
  - `resilience_smoke`
  - `timeout_smoke`
  - `socket_live_viewer_timeout_smoke`
  - `socket_live_snapshot_batching_smoke`
  - `server_inprocess_cli_smoke`
  - `server_socket_live_cli_smoke`
  - `server_socket_live_idle_cli_smoke`
  - `server_process_live_smoke`
  - `server_process_live_json_smoke`
  - `server_process_live_override_precedence_smoke`
  - `server_process_live_run_forever_smoke`
  - `server_invalid_backend_cli`
  - `server_invalid_frame_format_cli`
  - `server_invalid_port_cli`
  - `server_invalid_bind_host_cli`
  - `server_invalid_heartbeat_cli`
  - `server_invalid_batch_cli`

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
