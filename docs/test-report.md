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
- process-level live smoke covers post-engagement review response and automatic post-assessment archive behavior
- process-level live smoke covers both binary and JSON TCP framing
- the fire control console executable drives a scripted `socket_live` flow over TCP
- the SDL GUI viewer attaches to the live UDP path and is covered by a headless smoke
- the repository ships a one-command live demo script with smoke coverage
- the GUI control panel covers reset-to-standby and restart-after-reset behavior
- command acceptance now has explicit GUI-side visual feedback and dedicated smoke coverage
- the GUI now uses state-dependent `standby_setup`, `live_tactical`, and `review_tactical` layouts so the tactical picture remains the primary feature
- GUI headless smoke asserts phase banner and authoritative decision semantics through dump-state fields
- a dedicated GUI layout smoke verifies standby layout, setup visibility, and map-dominant panel sizing
- live GUI dump-state now exposes real `link_delay_ms`, raw RTT sample, and authoritative `picture_age_ms`
- GUI review smoke verifies that Review is requested as a post-assessment/post-archive action rather than a primary live control
- the GUI routes review output through the bottom log panel and removes full-time setup/link panels from review mode
- the GUI live timeline now includes control acknowledgements alongside server events so the log panel remains useful before the next telemetry update
- user-facing viewer text now presents the interceptor as an interceptor
- the GUI exposes direct scenario parameter controls and a dense world-space tactical map
- a GUI parameter smoke verifies that direct timeout/speed adjustments change parameters and produce `timeout_observed`
- a dedicated GUI setup smoke verifies that editing setup during a live run only changes the next-start plan and does not rewrite the active scenario state
- the pre-command `Acquire Track` / `Drop Track` toggle is verified across simulation, transport, and GUI flows
- interceptor launch now starts from fixed origin `(0,0)` with configurable launch angle, and config/payload/runtime validation covers that path
- tracking output now exposes actual tracker residual/covariance/measurement-age/missed-update values in viewer output and payloads instead of a fake confidence percentage
- tracker observation cadence now produces real non-zero measurement age between scheduled updates and deterministic missed-update counts on dropped samples, so those fields are meaningfully exercised in runtime/tests
- a dedicated GUI auto-rejoin smoke verifies that the viewer can recover from a timeout by reissuing `session_join`
- a dedicated GUI repo-root smoke verifies that the setup panel preloads scenario parameters from the provided runtime config root
- a dedicated GUI malformed-udp smoke verifies that malformed UDP datagrams are ignored without advancing viewer picture-status timestamps or mutating timeline state
- the same malformed-udp smoke also verifies that the viewer ignores datagrams from unexpected UDP endpoints
- a dedicated GUI snapshot-ordering smoke verifies that out-of-order snapshots and older telemetry samples do not roll back viewer state
- scenario flow coverage now also verifies that `reset_session()` resets logical tick to zero without rolling snapshot sequence or snapshot timestamps backward
- baseline scenario flow now also verifies that the no-arg `run_baseline_demo()` follows the repository runtime config rather than drifting to struct-default values
- a dedicated example-output smoke verifies that `SimulationSession::write_example_output()` works with a plain relative filename that has no parent directory component
- a dedicated post-engagement review artifact smoke verifies that `SimulationSession::write_aar_artifacts()` works when asked to emit directly into the current working directory
- a dedicated GUI relative-dump-path smoke verifies that `--dump-state` and `--dump-frame` work with plain filenames in the current working directory
- a dedicated GUI duplicate-start smoke verifies that a rejected second `Start` does not overwrite the active authoritative scenario picture with a new local randomization
- the fire control console now loads scenario-start parameters from `--repo-root` config instead of falling back to baked defaults
- command-console live flow now bounds its review polling window from scenario timeout parameters instead of using a fixed short retry budget
- the one-command live demo script now configures/builds required binaries and clears repo-local stale demo processes before launch
- the live demo smoke now mutates the runtime-root scenario config to prove the script forwards that config path through to the fire control console
- the server CLI now validates `--sample-mode tracked_intercept|unguided_intercept` for deterministic in-process artifact generation
- tracked_intercept and unguided_intercept sample modes are both covered by artifact and determinism smoke coverage
- the live demo script now includes a regeneration mode that refreshes tracked_intercept/unguided_intercept artifacts and GUI screenshots in one command
- the GUI viewer now defaults to a 100 ms heartbeat interval so live socket demo progression is not lost to stale viewer keepalive timing
- the simulation now emits continuous world-space positions, velocity/heading metadata, predicted intercept data, and seeker/FOV state
- successful intercept now deactivates the target before archive and regression coverage asserts that state change
- each GUI `Start` now applies a small bounded random jitter to actual target start geometry/target velocity while preserving the displayed setup baseline and keeping the interceptor at its planned origin
- the GUI setup panel now exposes launch-angle control while keeping interceptor origin fixed
- the oversized GUI translation unit was split into focused support/network/control/render modules without changing behavior
- process-level live smoke verifies artifact/log generation for executable live mode
- long-running live mode handles signal-driven shutdown and flushes outputs
- startup output exposes current viewer state and idle/no-snapshot conditions
- the shared protocol schema compiles and passes smoke checks
- concrete payload serialization/parsing round-trips successfully
- JSON and binary frame codecs round-trip successfully
- transport backend dispatch behavior is verified
- live transport session policy is verified
- scenario branching smoke verifies that command timing/kinematics can change the final assessment
- post-engagement review control semantics are verified
- the scenario flow passes end-to-end regression
- invalid command ordering is rejected and logged
- duplicate start/stop and post-archive control requests are rejected and logged
- runtime config loading is verified against the example config set
- runtime artifact paths are verified against configurable repo roots
- runtime artifacts and logs expose stable schema/version metadata
- baseline artifact generation is deterministic across repeated runs
- session summary is emitted in both Markdown and JSON forms
- runtime session logging is verified against generated structured log output
- server CLI success smokes now run against temp runtime roots so regression runs do not overwrite the checked-in sample bundle
- tracked_intercept/unguided_intercept viewer-state goldens are emitted as reduced JSON artifacts so visual intent can be checked semantically without relying on BMP byte identity
- replay cursor stepping is verified against viewer output
- resilience/replay rendering behavior passes smoke verification
- live viewer heartbeat timeout behavior is verified against the socket backend
- live viewer `Link Delay` now comes from heartbeat RTT rather than receive-gap heuristics
- live viewer `Picture Age` now comes from authoritative snapshot wall-clock capture time
- telemetry payloads now align event summaries to the snapshot tick they accompany, and a dedicated smoke verifies that alignment
- live viewer picture-status transitions are verified against the socket backend
- degraded picture status under packet loss is verified in rendered output
- live snapshot batching/filtering is verified against the socket backend
- a dedicated drift smoke regenerates tracked_intercept/unguided_intercept text artifacts plus viewer-state goldens into a temp runtime root and verifies they still match the checked-in canonical bundle

## Latest Result

- configure: passed
- build: passed
- test: passed (`54/54` tests)
- runtime smoke: passed (`icss_server`, `icss_fire_control_console`, `icss_tactical_display`)
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
  - `fire_control_console_socket_live_smoke`
  - `tactical_display_gui_live_smoke`
  - `tactical_display_gui_layout_modes_smoke`
  - `live_demo_script_smoke`
  - `sample_regen_script_smoke`
  - `checked_in_sample_bundle_drift_smoke`
  - `server_invalid_backend_cli`
  - `server_invalid_sample_mode_cli`
  - `server_invalid_frame_format_cli`
  - `server_invalid_port_cli`
  - `server_invalid_bind_host_cli`
  - `server_invalid_heartbeat_cli`
  - `server_invalid_batch_cli`

## Acceptance Checks

### 1. Scenario Completeness
- [x] scenario runs from start to post-engagement review
- [x] command validation occurs on server
- [x] final assessment is produced by server

### 2. Multi-Client Coverage
- [x] fire control console participates
- [x] tactical display receives state/telemetry updates
- [x] roles are meaningfully separated

### 3. Protocol Coverage
- [x] TCP responsibilities exercised
- [x] UDP snapshot/telemetry responsibilities exercised
- [x] transport split documented clearly

### 4. Resilience Coverage
- [x] at least one abnormal case is exercised
- [x] logs show the abnormal case clearly
- [x] system behavior after the abnormal case is understandable

### 5. Post-Engagement Review Coverage
- [x] one sample session can be replayed/reconstructed
- [x] event timeline includes key command/assessment points

### 6. Operational Coverage
- [x] config separation exists
- [x] logs have meaningful structure
- [x] session cleanup/end-state is visible

## Related Outputs

- sample review summary: `assets/sample-aar/session-summary.md`
- sample review summary JSON: `assets/sample-aar/session-summary.json`
- replay timeline: `assets/sample-aar/replay-timeline.json`
- unguided_intercept review summary: `assets/sample-aar/unguided_intercept/session-summary.md`
- unguided_intercept review summary JSON: `assets/sample-aar/unguided_intercept/session-summary.json`
- unguided_intercept replay timeline: `assets/sample-aar/unguided_intercept/replay-timeline.json`
- unguided_intercept sample output: `examples/sample-output-unguided_intercept.md`
- viewer-state goldens: `assets/screenshots/tactical-display-tracked_intercept-state.json`, `assets/screenshots/tactical-display-unguided_intercept-state.json`
- runtime log: `logs/session.log`
- GUI screenshots: `assets/screenshots/tactical-display-tracked_intercept.bmp`, `assets/screenshots/tactical-display-unguided_intercept.bmp`
- protocol doc: `docs/protocol.md`
- concrete build/test commands: see canonical commands above
