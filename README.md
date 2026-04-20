# Interceptor Command Simulation System

> C++ authoritative simulation system with role-separated clients, a protocol-defined TCP/UDP split, and AAR-focused observability.

## Overview

Interceptor Command Simulation System is a **C++ real-time simulation/control system**.

Core characteristics:
- C++ real-time server/software engineering
- server-authoritative state management
- protocol-defined TCP/UDP role separation
- multi-client command/viewer handling
- resilience under abnormal network conditions
- replayable logging / AAR (After Action Review)
- operability and maintainable component boundaries

## Implementation State

This repository contains an implemented system with deterministic local verification and a separate live transport path for socket-based exchange.

- documented runtime, protocol, and operations boundaries
- CMake-based configure/build/test flow
- shared protocol schema plus payload/frame codecs
- deterministic in-process runtime plus socket-based transport backend
- executable server modes for `in_process` and `socket_live`
- process-level live smoke for the executable `socket_live` server path
- live server mode writes runtime log and AAR/sample outputs when snapshots exist
- single-session live transport policy with one fire control console and one tactical display
- command, snapshot, telemetry, AAR, and timeout flows exercised in code
- regression coverage for protocol, runtime, transport, logging, replay, and resilience paths

The default runtime remains **in-process** for deterministic verification, while a separate live socket backend provides TCP/UDP bind/listen behavior, command/AAR exchange, heartbeat handling, and configurable snapshot delivery.
Both server modes now print backend, bind, heartbeat, and delivery settings at startup.

## Focus

- **Primary focus:** server-side / real-time systems engineering
- **Secondary focus:** state management, command handling, situation propagation, and operability thinking

## Scope

### Runtime Shape
- 1 C++ server
- 2 clients
  - fire control console
  - minimal 2D tactical display
- 1 representative scenario
- server-authoritative validation and assessment
- TCP/UDP split with documented responsibilities
- replayable event logging / AAR
- at least 1 resilience case
  - reconnect, timeout, or UDP loss recovery

### Viewer Surface
- target / interceptor position icons
- track state
- tracker estimate/covariance state
- connection status
- freshness state
- tick / latency / packet loss telemetry
- snapshot sequence
- last snapshot timestamp
- event log panel
- AAR playback cursor
- interceptor status / engage order status / assessment state

### Explicit Non-Goals
- flashy effects or entertainment-oriented presentation
- direct action controls such as WASD/manual aiming
- progression systems (items, ranking, leveling)
- precise real-world weapons physics or tactics
- multi-project spread

## Outputs
- core documentation set under `docs/`
- generated AAR outputs under `assets/sample-aar/`
- generated sample output under `examples/`

## Guide

- `docs/architecture.md` — component boundaries and state authority
- `docs/protocol.md` — TCP/UDP responsibilities and message families
- `docs/scenario.md` — representative scenario and state flow
- `docs/operations.md` — logging, configuration, resilience, cleanup
- `docs/aar.md` — event schema and replay/AAR design
- `docs/test-report.md` — verification plan and result log
- `docs/design-faq.md` — public design rationale and common questions
- `docs/walkthrough.md` — recommended system walkthrough
- `docs/implementation-checklist.md` — implementation checklist
- `docs/reviewer-brief.md` — short review entrypoint
- `docs/discussion-guide.md` — discussion points and expected questions

## Quick Review Path

1. `./build/icss_server --backend in_process`
2. `assets/sample-aar/session-summary.md`
3. `examples/sample-output.md`
4. `assets/sample-aar/unguided_intercept/session-summary.md`
5. `examples/sample-output-unguided_intercept.md`
6. `docs/protocol.md`
7. `docs/test-report.md`

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

### Server

```bash
./build/icss_server --backend in_process
./build/icss_server --backend socket_live --tick-limit 2
./build/icss_server --backend socket_live --tcp-port 0 --udp-port 0 --tick-limit 40 --tick-sleep-ms 20
./build/icss_server --backend socket_live --run-forever --tick-sleep-ms 20
./build/icss_server --backend socket_live --bind-host 127.0.0.1 --tcp-port 0 --udp-port 0
./build/icss_server --backend socket_live --tick-rate-hz 30 --telemetry-interval-ms 150 --heartbeat-interval-ms 600 --heartbeat-timeout-ms 1800 --udp-max-batch-snapshots 1 --udp-send-latest-only true --max-clients 4
```

### Live Clients

```bash
./build/icss_tactical_display_gui --host 127.0.0.1 --udp-port 4001
./build/icss_fire_control_console --backend socket_live --host 127.0.0.1 --tcp-port 4000
./build/icss_fire_control_console --backend socket_live --host 127.0.0.1 --tcp-port 4000 --repo-root /path/to/repo-root
```

`icss_tactical_display_gui` is not just a position plot. The window emphasizes:
- the current mission phase/state-machine step
- the latest authoritative server decision or rejection reason via a compact status badge and `Authoritative Status` panel
- resilience/telemetry state (`fresh`, `degraded`, `resync`, `stale`)
- a terminal-style server event log
- the tactical picture with target/interceptor geometry as supporting context
- a larger 2304x1536 world-space picture rather than a tiny fixed board
- target/interceptor velocity, heading, predicted intercept point, TTI, and seeker/FOV state
- the interceptor now always starts from world origin `(0,0)` and uses a configurable launch angle with a `45 deg` default
- conventional tactical-picture styling: entity identity by color/shape, solid arrows for current motion, dashed trails for history, orange engagement link, and green diamond/X predicted intercept marker
- the viewer now uses an explicit viewport transform and float world-history so vectors, trails, and markers are rendered from world-space rather than snapped render-grid coordinates
- tracker estimate, latest measurement residual, covariance, measurement age, and missed-update state from a scheduled noisy-observation tracker instead of a fake tracking percentage in the UI; residual stays visible while age increases between updates
- time-of-command outcome branching: the same scenario can end in `intercept_success` or `timeout_observed` depending on timing and kinematics
- on `intercept_success`, the authoritative runtime deactivates the target and freezes both target/interceptor motion before the run auto-archives
- each GUI `Start` randomizes the actual target start geometry and target velocity within a small bounded envelope around the planned setup values, while keeping the interceptor origin anchored to its planned coordinates
- the GUI exposes `Track`, with `Acquire Track` / `Drop Track` stateful labeling, as the operator-facing pre-launch track toggle; protocol/internal names remain `track_acquire` / `track_drop`, and track control locks once `Engage` launches the interceptor
- the GUI now uses an explicit camera/viewport transform: world origin is treated as bottom-left with +x right / +y up, and the renderer flips Y only when mapping world coordinates into SDL screen space

Defaults:
- `icss_server --backend socket_live` uses `json` TCP framing unless `--tcp-frame-format` overrides it
- `icss_tactical_display_gui` and `icss_fire_control_console` now default to the same `json` framing
- `icss_tactical_display_gui` now defaults to a 100 ms heartbeat interval so live progression does not stall waiting on viewer keepalive traffic
- if the server uses `--tcp-frame-format binary`, pass `--tcp-frame-format binary` to the GUI viewer and fire control console too
- `icss_fire_control_console` can use `--repo-root` to load the same scenario config as the server before sending `scenario_start`
- `run_live_demo.sh` forwards `--runtime-root` to the fire control console, so a custom runtime config changes both server and console behavior instead of only the server

For a manual live run:
1. start `icss_server --backend socket_live --run-forever`
2. start `icss_tactical_display_gui`
3. run `icss_fire_control_console --backend socket_live ...` to drive the scripted command flow

### One-Command Live Demo

```bash
./scripts/run_live_demo.sh
```

`run_live_demo.sh` now does two defensive things by default:
- configures/builds the required demo binaries before launch
- kills existing repo-local `icss_server`, `icss_tactical_display_gui`, and `icss_fire_control_console` processes so you do not end up looking at an old window or stale server

Opt out only if you need it:
```bash
./scripts/run_live_demo.sh --skip-build --preserve-existing
```

Default behavior:
- starts `icss_server --backend socket_live --run-forever`
- starts `icss_tactical_display_gui`
- leaves control to the GUI panel
- GUI live control order: `Start -> Track -> Ready -> Engage -> AAR -> Reset -> Start`
- `AAR` is intentionally separate from the live control chain; request it after assessment/archive to inspect server-side AAR data
- the bottom timeline panel now shows live server events plus control acknowledgements; `AAR` switches that panel into post-action AAR mode
- the timeline panel now clips overflowing log lines, draws a scrollbar, and supports mouse-wheel / PageUp / PageDown / Home / End scrolling
- the GUI highlights mission phase, authoritative decision state, and resilience telemetry while you step through the flow

For a fully scripted visible run:

```bash
./scripts/run_live_demo.sh --scripted
```

Headless mode also runs the scripted fire control console flow automatically and prints the artifact summary when it completes.

Useful options:

```bash
./scripts/run_live_demo.sh --frame-format json
./scripts/run_live_demo.sh --tcp-port 0 --udp-port 0
./scripts/run_live_demo.sh --headless --viewer-duration-ms 1500
```

Regenerate the checked-in tracked_intercept/unguided_intercept comparison bundle:

```bash
./scripts/run_live_demo.sh --regen-samples --sample-mode all
```

That command refreshes:
- tracked_intercept AAR/sample output under `assets/sample-aar/` and `examples/sample-output.md`
- unguided_intercept comparison artifacts under `assets/sample-aar/unguided_intercept/` and `examples/sample-output-unguided_intercept.md`
- deterministic viewer-state goldens under `assets/screenshots/tactical-display-tracked_intercept-state.json` and `assets/screenshots/tactical-display-unguided_intercept-state.json`
- GUI screenshots under `assets/screenshots/tactical-display-tracked_intercept.bmp` and `assets/screenshots/tactical-display-unguided_intercept.bmp`

The regression suite now keeps repo-root review assets stable:
- server CLI success smokes run against temp runtime roots instead of writing into the checked-in bundle
- a dedicated drift smoke regenerates tracked_intercept/unguided_intercept text artifacts and viewer-state goldens into a temp runtime root and checks them against the checked-in canonical files

### Artifact Summary

```bash
./build/icss_artifact_summary
```

`icss_artifact_summary` now reports the tracked_intercept bundle by default, adds unguided_intercept comparison lines when `assets/sample-aar/unguided_intercept/` is present, and also supports explicit compare mode via `--tracked_intercept-root PATH --unguided_intercept-root PATH`.

## Code Paths

- `CMakeLists.txt` — root build/test entrypoint
- `common/include/icss/protocol/messages.hpp` — shared protocol/message schema
- `common/include/icss/protocol/payloads.hpp` — concrete payload structs
- `common/include/icss/protocol/serialization.hpp` — textual payload serialization/parse helpers
- `common/include/icss/net/transport.hpp` — transport abstraction and backend factory
- `common/include/icss/core/` — shared session/domain types and simulation API
- `common/src/` — config loader, split transport backends, runtime orchestration, split simulation/runtime lifecycle, AAR writer, ASCII tactical display renderer
- `server/src/main.cpp` — authoritative reference entrypoint
- `clients/command-console/src/main.cpp` — fire control console reference flow and socket-live client path
- `clients/tactical-viewer/src/main.cpp` — minimal 2D tactical display reference flow
- `clients/tactical-viewer-gui/src/main.cpp` — SDL-based live tactical display window
- `clients/tactical-viewer-gui/src/app_*.cpp` — split GUI support, setup, visual-state, render, and network helpers
- `tests/protocol/src/protocol_smoke.cpp` — protocol smoke verification
- `tests/protocol/src/payload_codec_smoke.cpp` — payload serialization regression
- `tests/protocol/src/frame_codec_smoke.cpp` — JSON/binary frame codec regression
- `tests/protocol/src/transport_backend_smoke.cpp` — transport backend regression
- `tests/scenario/src/scenario_flow.cpp` — end-to-end scenario regression
- `tests/scenario/src/validation_rejects_invalid_flow.cpp` — invalid command-order regression
- `tests/scenario/src/runtime_config_smoke.cpp` — config loading regression
- `tests/scenario/src/runtime_artifact_paths_smoke.cpp` — artifact path regression
- `tests/scenario/src/session_log_smoke.cpp` — structured session log regression
- `tests/scenario/src/replay_cursor_smoke.cpp` — replay cursor/viewer regression
- `tests/resilience/src/resilience_smoke.cpp` — reconnect/rendering resilience regression
- `tests/resilience/src/timeout_smoke.cpp` — timeout visibility regression
- `tests/resilience/src/socket_live_viewer_timeout_smoke.cpp` — live viewer heartbeat timeout regression
- `tests/resilience/src/socket_live_snapshot_batching_smoke.cpp` — live snapshot batching/filtering regression

## High-Level Repo Layout

```text
server/
clients/
  command-console/
  tactical-viewer/
docs/
configs/
tests/
  scenario/
  protocol/
  resilience/
assets/
  diagrams/
  screenshots/
  sample-aar/
examples/
```

## Start Here

Start with `docs/implementation-checklist.md` and lock:
1. component boundaries
2. protocol categories
3. event schema
4. scenario timeline
5. tactical display telemetry contract
