# Tactical Viewer GUI

Role:
- live window-based tactical display
- attach to the `socket_live` UDP path
- render read-only state, telemetry, and local viewer events

Guardrails:
- remains observability-first
- does not issue commands
- avoids game-style control loops or effects

Entrypoint:
- `clients/tactical-viewer-gui/src/main.cpp`

Current behavior:
- opens an SDL window
- sends UDP `session_join` and heartbeat traffic
- receives `world_snapshot`, `telemetry`, and `display_heartbeat_ack`
- uses the tactical picture as the primary surface rather than a side panel companion
- keeps a compact authoritative status badge (`INTERCEPTING`, `REJECTED`, `ASSESSED`, `LIVE`, etc.) in the header
- keeps server event history in a bottom event/review panel
- keeps heartbeat traffic out of the event log and reports it as telemetry instead
- shows real `Link Delay` from heartbeat RTT and real `Picture Age` from authoritative snapshot wall-clock capture time
- auto-attaches the control channel when the first control button is used
- provides `Start`, `Track`, `Ready`, `Engage`, `Reset`, and a separate `Review` action
- automatically archives the run after assessment; `Review` is post-assessment/post-archive inspection rather than a primary live-control step
- uses three presentation modes
  - `standby_setup`: preview plus next-start setup controls
  - `live_tactical`: map-first live engagement picture with compact summary/control/link cards
  - `review_tactical`: map-first archived/review picture with the review timeline promoted
- uses the bottom panel as an event timeline: live server events plus control acknowledgements during operation, and review history after `Review`
- clips overflowing timeline lines, shows a scrollbar when needed, and supports mouse-wheel / `PageUp` / `PageDown` / `Home` / `End` scrolling
- exposes direct scenario setup controls for target start position, target velocity, launch angle, interceptor speed, and timeout before the next `Start`, but only keeps that editor open in `standby_setup`
- each GUI `Start` randomizes the actual target start geometry and target velocity within a small bounded envelope around the planned setup values, while keeping the interceptor anchored to its planned origin point
- keeps the interceptor fixed at world origin `(0,0)` and launches at a configurable base angle with `45 deg` as the default
- uses `Acquire Track` / `Drop Track` as a pre-launch track toggle; once `Engage` launches the interceptor, track control locks for the rest of the run
- uses an explicit camera/viewport transform with world origin at bottom-left (`+x` right, `+y` up); screen rendering flips only the Y axis at the final world→screen mapping step
- supports optional observability-only camera controls via CLI (`--camera-zoom`, `--camera-pan-x`, `--camera-pan-y`) without turning the viewer into a direct-control surface
- keeps scenario setup changes scoped to the next `Start`; editing setup during a live run does not rewrite the active tactical picture
- renders a larger 2304x1536 world-space picture instead of the earlier toy-sized grid
- uses role-vs-meaning visual conventions in the tactical picture: red square for target, blue diamond for interceptor, solid arrow vectors for current motion, dashed trails for history, orange engagement line for active command, and green diamond/X marker for predicted intercept
- uses map overlays for current assessment, track state, TTI, and review status instead of large descriptive side panels
- keeps motion arrows, engagement line, and predicted-intercept marker live-only: they disappear once execution stops or the run moves into post-assessment state, while dashed trails remain as history
- automatically reissues `session_join` after stale telemetry so the viewer can recover from a live timeout without restarting the process
- accepts `--repo-root` so the viewer's setup panel preloads the same scenario config the server will run
- supports reset-to-standby followed by a fresh `Start`
- gives immediate visual command feedback by highlighting the intercept line/target box and updating control state locally on accepted actions
- supports `--headless --dump-state` for automated smoke coverage
