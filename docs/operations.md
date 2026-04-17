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

## Operational Signals

Operational records should make the following concerns obvious:
- monitoring
- cleanup
- reproducibility
- traceability
- post-run analysis
