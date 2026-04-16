# Interceptor Command Simulation System

> C++ authoritative simulation baseline with multi-client roles, a protocol-defined TCP/UDP split, and AAR-focused observability.

## Project Overview

Interceptor Command Simulation System is a **focused real-time simulation/control project**.

The goal is **not** to present a consumer game. The goal is to present a **battlefield-system style real-time simulation/control system** that demonstrates:
- C++ real-time server/software engineering
- server-authoritative state management
- protocol-defined TCP/UDP role separation
- multi-client command/viewer handling
- resilience under abnormal network conditions
- replayable logging / AAR (After Action Review)
- operability and explanation quality suitable for design review and maintenance

## Repository Status

This repository is currently in **execution-prep / early implementation** stage.

- documentation scaffold is committed
- placeholder config/examples are committed
- canonical C++ build/test commands are committed
- a minimal C++/CMake skeleton is committed
- a shared protocol schema header is committed at `common/include/icss/protocol/messages.hpp`
- local configure/build/test verification has passed for the current skeleton
- payload serialization, config loading, and reusable runtime separation are committed
- regression coverage now includes protocol, payload codec, scenario flow, invalid command rejection, runtime config loading, resilience smoke, and timeout visibility

This is still an early implementation baseline, but the repository now supports configure/build/test verification.
The current runtime is **in-process** and uses a committed protocol schema to model TCP/UDP responsibilities; live socket transport has not been added yet.

## Project Focus

- **Primary focus:** server-side / real-time systems engineering
- **Secondary focus:** state management, command handling, situation propagation, and operability thinking

## Locked 4-Week Baseline

### Must Implement
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

### Must Show in the 2D Tactical Viewer
- target / asset position icons
- tracking status
- connection status
- tick / latency / packet loss telemetry
- last snapshot timestamp
- event log panel
- AAR playback cursor

### Explicit Non-Goals
- flashy effects or game-like presentation
- direct action controls such as WASD/manual aiming
- progression systems (items, ranking, leveling)
- precise real-world weapons physics or tactics
- multi-project spread

## Deliverables
- GitHub repository
- 3–5 minute demo video
- core documentation set under `docs/`
- sample AAR evidence under `assets/sample-aar/`

## Repository Guide

- `docs/architecture.md` — component boundaries and state authority
- `docs/protocol.md` — TCP/UDP responsibilities and message families
- `docs/scenario.md` — representative scenario and state flow
- `docs/operations.md` — logging, configuration, resilience, cleanup
- `docs/aar.md` — event schema and replay/AAR design
- `docs/test-report.md` — verification plan and evidence ledger
- `docs/design-faq.md` — public design rationale and common questions
- `docs/demo-script.md` — recommended demo flow
- `docs/week1-checklist.md` — immediate Week 1 execution checklist

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

## Current Code Skeleton

- `CMakeLists.txt` — root build/test entrypoint
- `common/include/icss/protocol/messages.hpp` — shared protocol/message schema
- `common/include/icss/protocol/payloads.hpp` — concrete payload structs
- `common/include/icss/protocol/serialization.hpp` — textual payload serialization/parse helpers
- `common/include/icss/core/` — shared session/domain types and simulation API
- `common/src/` — config loader, runtime orchestration, simulation runtime, AAR writer, ASCII tactical viewer renderer
- `server/src/main.cpp` — baseline authoritative demo entrypoint
- `clients/command-console/src/main.cpp` — command console baseline flow
- `clients/tactical-viewer/src/main.cpp` — minimal 2D tactical viewer baseline
- `tests/protocol/src/protocol_smoke.cpp` — protocol smoke verification
- `tests/protocol/src/payload_codec_smoke.cpp` — payload serialization regression
- `tests/scenario/src/scenario_flow.cpp` — end-to-end baseline scenario regression
- `tests/scenario/src/validation_rejects_invalid_flow.cpp` — invalid command-order regression
- `tests/scenario/src/runtime_config_smoke.cpp` — config loading regression
- `tests/scenario/src/runtime_artifact_paths_smoke.cpp` — artifact path regression
- `tests/resilience/src/resilience_smoke.cpp` — reconnect/rendering resilience regression
- `tests/resilience/src/timeout_smoke.cpp` — timeout visibility regression

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

## Immediate Next Step

Start with `docs/week1-checklist.md` and lock:
1. component boundaries
2. protocol categories
3. event schema
4. scenario timeline
5. tactical viewer telemetry contract
