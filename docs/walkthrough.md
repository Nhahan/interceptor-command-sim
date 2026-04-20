# Reference Walkthrough

## Goal

Show command/control flow, state propagation, one resilience case, and AAR outputs.

## Outline

### 0:00–0:20 — Project framing
- what this system is
- what operating model it emphasizes
- what was intentionally not built

### 0:20–0:50 — Components
- server
- fire control console
- 2D tactical display
- backend selection (in-process by default, socket-live backend for network transport)
- frame selection (JSON vs binary) on the TCP command path
- startup settings (bind host, heartbeat, delivery policy)
- idle live state (`display_connection=disconnected`, `latest_snapshot_sequence=none`) before any active exchange
- executable live server process can be exercised with external TCP/UDP clients

### 0:50–1:40 — Scenario start
- start session (GUI setup values are used as a base; actual start geometry is slightly randomized per run)
- detect target and acquire track
- show positions/status on viewer

### 1:40–2:30 — Command and assessment
- issue command from console or GUI
- show server-side validation/assessment result; on success the target should disappear/deactivate before archive
- keep telemetry visible
- point out richer state panels (track measurement residual / covariance / interceptor status / engage order status / assessment code)
- point out connection and freshness labels on the viewer

### 2:30–3:20 — Resilience case
- exercise reconnect / timeout / UDP loss convergence
- surface viewer heartbeat/timeout visibility on the live transport path when appropriate
- point out `freshness=degraded` when packet loss is visible but the stream is still current
- point out `freshness=resync` and `freshness=stale` transitions when applicable
- point out snapshot batching/filtering behavior when a viewer joins late
- explain what the viewer and logs show

### 3:20–4:20 — AAR
- open replay/AAR artifact
- compare tracked_intercept and unguided_intercept launch artifacts side by side
- point to timeline, event log, track state, intercept profile, and assessment outputs
- point to freshness and latest snapshot metadata in the generated summaries

### Closing
- reiterate why the architecture was chosen
- reiterate what was intentionally excluded

## Recording Checklist

- keep one terminal for server startup output
- keep one terminal for fire control console actions
- keep one terminal or window for the tactical display/sample output
- prepare the AAR session summary and replay timeline in advance
- avoid editing files live during the recording

## Shot List

1. server startup
2. command sequence
3. tactical display state
4. one resilience moment
5. tracked_intercept and unguided_intercept AAR summaries / replay timelines
6. checked-in GUI screenshots (`assets/screenshots/tactical-display-tracked_intercept.bmp`, `assets/screenshots/tactical-display-unguided_intercept.bmp`)

## Minimal Interceptor Set

- server startup output
- one command sequence
- one viewer frame with freshness labels
- one resilience example
- one tracked_intercept AAR summary
- one unguided_intercept AAR summary
- one checked-in GUI screenshot under `assets/screenshots/`
