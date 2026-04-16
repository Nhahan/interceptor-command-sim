# Reference Walkthrough

## Goal

Show command/control flow, state propagation, one resilience case, and AAR outputs.

## Outline

### 0:00–0:20 — Project framing
- what this system is
- what operating model it emphasizes

### 0:20–0:50 — Components
- server
- command console
- 2D tactical viewer
- backend selection (in-process by default, socket-live backend for network transport)
- frame selection (JSON vs binary) on the TCP command path

### 0:50–1:40 — Scenario start
- start session
- detect/track target
- show positions/status on viewer

### 1:40–2:30 — Command and judgment
- issue command from console
- show server-side validation/judgment result
- keep telemetry visible
- point out richer state panels (track confidence / asset status / command lifecycle / judgment code)

### 2:30–3:20 — Resilience case
- exercise reconnect / timeout / UDP loss convergence
- surface viewer heartbeat/timeout visibility on the live transport path when appropriate
- point out snapshot batching/filtering behavior when a viewer joins late
- explain what the viewer and logs show

### 3:20–4:20 — AAR
- open replay/AAR artifact
- point to timeline, event log, and judgment outputs

### Closing
- reiterate why the architecture was chosen
- reiterate what was intentionally excluded
