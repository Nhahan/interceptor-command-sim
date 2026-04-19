# Tactical Viewer GUI

Role:
- live window-based tactical viewer
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
- receives `world_snapshot` and `telemetry`
- renders the tactical picture plus a mission phase rail, an `Authoritative Status` panel, resilience/telemetry panel, and terminal-style server log
- shows a compact authoritative status badge (`ENGAGING`, `REJECTED`, `JUDGED`, `LIVE`, etc.) in the header instead of burying long status text in panel body content
- shows server event history in a larger terminal-style log panel
- keeps heartbeat traffic out of the event log and reports it as telemetry instead
- auto-attaches the control channel when the first control button is used
- provides `Start`, `Guidance`, `Activate`, `Command`, `Reset`, and a separate `Review` action
- automatically archives the run after judgment; `Review` is post-judgment/post-archive AAR inspection rather than a primary live-control step
- uses the bottom panel as an event timeline: live server events plus control acknowledgements during operation, and a separate review mode after `Review`
- keeps the event timeline in a fixed terminal-style color scheme so mode changes do not re-style the panel
- clips overflowing timeline lines, shows a scrollbar when needed, and supports mouse-wheel / `PageUp` / `PageDown` / `Home` / `End` scrolling
- highlights the next recommended control based on the current authoritative phase instead of relying on a static workflow hint
- exposes direct scenario setup controls for target start position, target velocity, launch angle, interceptor speed, and timeout before the next `Start`
- each GUI `Start` randomizes the actual target start geometry and target velocity within a small bounded envelope around the planned setup values, while keeping the interceptor anchored to its planned origin point
- keeps the interceptor fixed at world origin `(0,0)` and launches at a configurable base angle with `45 deg` as the default
- uses `Guidance On` / `Guidance Off` as a pre-command guidance toggle; once `Command` launches the interceptor, guidance control locks for the rest of the run
- uses an explicit camera/viewport transform with world origin at bottom-left (`+x` right, `+y` up); screen rendering flips only the Y axis at the final world→screen mapping step
- supports optional observability-only camera controls via CLI (`--camera-zoom`, `--camera-pan-x`, `--camera-pan-y`) without turning the viewer into a direct-control surface
- keeps scenario setup changes scoped to the next `Start`; editing setup during a live run does not rewrite the active tactical picture
- renders a larger 2304x1536 world-space picture instead of the earlier toy-sized grid
- computes tracking estimate, latest measurement residual, covariance, measurement age, and missed-update state from the current scheduled noisy-observation tracker instead of showing a fake percentage; the residual remains visible while age rises between measurement refreshes
- uses role-vs-meaning visual conventions in the tactical picture: red square for target, blue diamond for interceptor, solid arrow vectors for current motion, dashed trails for history, orange engagement line for active command, and green diamond/X marker for predicted intercept
- uses an explicit viewport transform and float world-history so tactical overlays are rendered from world-space rather than snapped render-grid coordinates
- keeps motion arrows, engagement line, and predicted-intercept marker live-only: they disappear once execution stops or the run moves into post-judgment state, while dashed trails remain as history
- automatically reissues `session_join` after stale telemetry so the viewer can recover from a live timeout without restarting the process
- accepts `--repo-root` so the viewer's setup panel preloads the same scenario config the server will run
- supports reset-to-initialized followed by a fresh `Start`
- gives immediate visual command feedback by highlighting the intercept line/target box and updating control state locally on accepted actions
- keeps the state-machine story explicit with `INITIALIZED -> DETECTING -> GUIDANCE LOCKED -> INTERCEPTOR READY -> COMMAND ACCEPTED -> ENGAGING -> JUDGMENT PRODUCED -> ARCHIVED`
- supports `--headless --dump-state` for automated smoke coverage
