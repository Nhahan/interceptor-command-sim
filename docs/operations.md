# Operations / Supportability

## Why This Exists

The project should feel like an operable system, not a throwaway demo.

## Required Operational Concerns

### Configuration Separation
Provide separate config domains for:
- server/network
- scenario
- logging

Current committed config entry points:
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

Current baseline log output:
- runtime writes `logs/session.log`
- AAR artifacts are written under `assets/sample-aar/`
- `logs/session.log` now contains one session-summary record plus structured event records

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
- tick
- latency
- packet loss estimate
- last snapshot timestamp

## Demonstration Value

Operational evidence is part of the project's message.
A collaborator should see that the system designer thought about:
- monitoring
- cleanup
- reproducibility
- traceability
- post-run analysis
