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
- command, snapshot, telemetry, AAR, and timeout flows exercised in code
- regression coverage for protocol, runtime, transport, logging, replay, and resilience paths

The default runtime remains **in-process** for deterministic verification, while a separate live socket backend provides TCP/UDP bind/listen behavior, command/AAR exchange, heartbeat handling, and configurable snapshot delivery.

## Focus

- **Primary focus:** server-side / real-time systems engineering
- **Secondary focus:** state management, command handling, situation propagation, and operability thinking

## Scope

### Runtime Shape
- 1 C++ server
- 2 clients
  - command console
  - minimal 2D tactical viewer
- 1 representative scenario
- server-authoritative validation and judgment
- TCP/UDP split with documented responsibilities
- replayable event logging / AAR
- at least 1 resilience case
  - reconnect, timeout, or UDP loss recovery

### Viewer Surface
- target / asset position icons
- tracking status
- tracking confidence
- connection status
- tick / latency / packet loss telemetry
- last snapshot timestamp
- event log panel
- AAR playback cursor
- asset status / command lifecycle / judgment state

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

## Code Paths

- `CMakeLists.txt` — root build/test entrypoint
- `common/include/icss/protocol/messages.hpp` — shared protocol/message schema
- `common/include/icss/protocol/payloads.hpp` — concrete payload structs
- `common/include/icss/protocol/serialization.hpp` — textual payload serialization/parse helpers
- `common/include/icss/net/transport.hpp` — transport abstraction and backend factory
- `common/include/icss/core/` — shared session/domain types and simulation API
- `common/src/` — config loader, transport backends, runtime orchestration, simulation runtime, AAR writer, ASCII tactical viewer renderer
- `server/src/main.cpp` — authoritative reference entrypoint
- `clients/command-console/src/main.cpp` — command console reference flow
- `clients/tactical-viewer/src/main.cpp` — minimal 2D tactical viewer reference flow
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
5. tactical viewer telemetry contract
