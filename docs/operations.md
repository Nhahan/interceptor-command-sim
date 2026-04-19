# Operations / Supportability

## Why This Exists

The project should behave like an operable system.

## Required Operational Concerns

### Configuration Separation
Provide separate config domains for:
- server/network
- scenario
- logging

Current config entry points:
- `configs/server.example.yaml`
- `configs/scenario.example.yaml`
- `configs/logging.example.yaml`
- loader: `common/src/config.cpp`

Validation rules:
- valid IPv4 `bind_host`
- positive tick and telemetry intervals
- non-negative TCP/UDP ports within range
- heartbeat timeout greater than or equal to heartbeat interval
- positive batch and client counts
- frame format limited to `json` or `binary`

Server entry surfaces:
- `./build/icss_server --backend in_process`
- `./build/icss_server --backend socket_live --tick-limit <N>`

Startup output should expose at least:
- backend selection
- bind host and active TCP/UDP ports
- frame format
- heartbeat interval/timeout
- delivery settings such as `udp_send_latest_only` and `udp_max_batch_snapshots`
- command and viewer connection state
- latest event type when available
- current viewer connection/freshness state
- latest snapshot sequence, or `none` when the server has not emitted a snapshot yet
- last snapshot timestamp for viewer-side staleness interpretation

Useful startup overrides for smoke and integration runs:
- `--tcp-port 0`
- `--udp-port 0`
- `--tick-limit <N>`
- `--tick-sleep-ms <N>`
- `--run-forever`
- `--tick-rate-hz <N>`
- `--telemetry-interval-ms <N>`
- `--heartbeat-interval-ms <N>`
- `--heartbeat-timeout-ms <N>`
- `--udp-max-batch-snapshots <N>`
- `--udp-send-latest-only true|false`
- `--max-clients <N>`

Precedence rule:
- CLI overrides take precedence over values loaded from `configs/*.yaml`

### Logging
Minimum expectations:
- structured enough to reconstruct timeline
- clear severity levels
- session lifecycle events recorded
- command acceptance/rejection recorded
- resilience events recorded

Current log output:
- runtime writes `logs/session.log`
- AAR artifacts are written under `assets/sample-aar/`
- `logs/session.log` now contains one session-summary record plus structured event records
- the summary log includes backend name, judgment code, and resilience summary
- runtime log and generated artifacts carry explicit schema/version fields

Live mode output parity:
- when snapshots exist, `socket_live` mode also writes session log, AAR artifacts, and sample output
- when no snapshot has been emitted yet, startup output reports that AAR artifacts were skipped
- during signal-driven shutdown, the server reports shutdown reason and flushes summary/output artifacts before exit
- the summary artifact is emitted in both Markdown and JSON forms
- successful intercepts deactivate the target before archive so post-run artifacts show the target as removed rather than still moving

Current live transport operational concerns:
- one active command console connection per runtime instance
- one active tactical viewer registration per runtime instance
- TCP command clients disconnect explicitly on EOF and clear stale sender state
- UDP viewer endpoints register separately from TCP command endpoints
- viewer heartbeat timeout can raise visible timeout state without contaminating authoritative judgment

### Session Cleanup
On shutdown or disconnect:
- release connection/session resources
- finalize pending logs
- mark session end reason

### Failure Handling
At minimum, make abnormal states visible for:
- timeout
- disconnect
- reconnect
- snapshot loss convergence

### Telemetry Visibility
The tactical viewer or command console should expose at least:
- connection status
- freshness state
- tick
- latency
- packet loss estimate
- snapshot sequence
- last snapshot timestamp
- richer track/asset/command/judgment state

Recommended freshness labels:
- `fresh` for current snapshots
- `degraded` when packet loss is visible but the stream is still current
- `resync` immediately after viewer reconnection
- `stale` when the viewer has timed out or fallen out of date

## Operational Signals

Operational records should make the following concerns obvious:
- monitoring
- cleanup
- reproducibility
- traceability
- post-run analysis
