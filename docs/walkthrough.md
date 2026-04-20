# Reference Walkthrough

## Goal

Show command/control flow, state propagation, one resilience case, and post-engagement review outputs.

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
- keep the tactical map centered as the primary visual
- point out that setup editing is only front-and-center in standby

### 1:40–2:30 — Command and assessment
- issue command from console or GUI
- show server-side validation/assessment result; on success the target should disappear/deactivate before archive
- use the side rail only for compact outcome/control context
- point out track state, TTI, and assessment overlays on the map

### 2:30–3:20 — Resilience case
- exercise reconnect / timeout / UDP loss convergence
- surface viewer heartbeat/timeout visibility on the live transport path when appropriate
- point out that `Link Delay` is real heartbeat RTT and `Picture Age` is authoritative snapshot wall-clock age
- point out `picture_status=degraded` when packet loss is visible but the stream is still current
- point out `picture_status=reacquiring` and `picture_status=stale` transitions when applicable
- point out that link/picture status is promoted only during live monitoring
- explain what the viewer and logs show

### 3:20–4:20 — Post-Engagement Review
- open replay/review artifact
- compare tracked_intercept and unguided_intercept launch artifacts side by side
- keep the archived map on screen and use the review timeline as the main secondary surface
- point to timeline, track state, intercept profile, and assessment outputs
- point to picture-status and latest snapshot metadata in the generated summaries

### Closing
- reiterate why the architecture was chosen
- reiterate what was intentionally excluded

## Recording Checklist

- keep one terminal for server startup output
- keep one terminal for fire control console actions
- keep one terminal or window for the tactical display/sample output
- prepare the review session summary and replay timeline in advance
- avoid editing files live during the recording

## Shot List

1. server startup
2. command sequence
3. tactical display state
4. one resilience moment
5. tracked_intercept and unguided_intercept review summaries / replay timelines
6. checked-in GUI screenshots (`assets/screenshots/tactical-display-tracked_intercept.bmp`, `assets/screenshots/tactical-display-unguided_intercept.bmp`)

## Minimal Interceptor Set

- server startup output
- one command sequence
- one viewer frame with picture-status labels
- one resilience example
- one tracked_intercept review summary
- one unguided_intercept review summary
- one checked-in GUI screenshot under `assets/screenshots/`
