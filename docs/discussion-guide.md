# Discussion Guide

## Core Message

This system is designed to show a clear separation between:
- server-owned truth
- client-issued intent
- state propagation
- post-run traceability

## Short Answers To Common Questions

### Why server-authoritative?
To keep validation and assessment in one place. The server is the only source of truth.

### Why split TCP and UDP?
Control traffic needs reliability and ordering. Snapshot traffic needs freshness more than per-packet guarantees.

### Why include AAR?
Because post-run analysis is part of the system contract. Commands, judgments, and resilience events should be explainable after execution.

### Why keep the tactical display read-only?
The viewer exists to expose state, telemetry, and replay information. It is an observability surface, not a direct-control surface.

### Why keep the live backend single-session?
The current scope optimizes for clear ownership, stable verification, and explainable behavior.

### How is resilience represented?
Through reconnect/resync, timeout visibility, degraded freshness under packet loss, and snapshot convergence.

## Longer Discussion Points

### 1. Transport Boundary
- `TransportBackend` separates runtime behavior from transport implementation
- `in_process` keeps deterministic local verification simple
- `socket_live` exercises actual TCP/UDP behavior

### 2. Replay / AAR
- event history is server-side
- replay cursor semantics are explicit
- live AAR requests return cursor and summary metadata

### 3. Operability
- config is split into server/scenario/logging
- startup output exposes live mode settings
- runtime log includes structured summary and event records

### 4. Validation
- runtime config is validated before execution
- invalid CLI overrides fail immediately with explicit messages
- regression tests cover both valid and invalid startup paths

## Trade-offs To State Clearly

### Why keep one scenario?
It keeps the repository focused on transport, authority, resilience, and replay rather than spreading effort across content.

### Why keep one fire control console and one viewer in live mode?
It keeps ownership clear and makes the live path easy to reason about and verify.

### Why allow both JSON and binary framing?
JSON is easier to inspect during development. Binary proves the framing boundary can be separated from the payload model.

### Why use snapshot convergence instead of complex recovery?
The current scope values clear state convergence over protocol complexity. The next valid snapshot is enough to recover viewer state.

## Evidence To Reference In Conversation

- `logs/session.log` for runtime summary and event records
- `assets/sample-aar/session-summary.md` for post-run summary
- `assets/sample-aar/replay-timeline.json` for replay/event history
- `examples/sample-output.md` for viewer-facing output
- `assets/sample-aar/unguided_intercept/session-summary.md` for the unguided_intercept-launch timeout comparison
- `examples/sample-output-unguided_intercept.md` for the viewer-facing unguided_intercept-launch comparison
- `docs/test-report.md` for the regression surface

## Good Closing Summary

“This repository is centered on a server-authoritative runtime, a clear TCP/UDP split, explicit resilience visibility, and replayable post-run analysis. The implementation keeps scope small on purpose and spends complexity budget on boundaries, verification, and explanation quality.”

## Good Files To Cite During Discussion

- `docs/architecture.md`
- `docs/protocol.md`
- `docs/operations.md`
- `docs/aar.md`
- `docs/test-report.md`

### Why randomize the actual start state from the GUI setup?
- it prevents every run from looking identical
- it preserves user-controlled base parameters while still demonstrating scenario sensitivity
- the jitter stays bounded so the run remains explainable and regression-friendly
