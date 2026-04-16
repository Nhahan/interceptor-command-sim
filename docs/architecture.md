# Architecture

## Objective

Define a server-authoritative real-time simulation architecture that is easy to explain in a design review and small enough to finish in 4 weeks.

## Core Components

### 1. Simulation Server
Responsibilities:
- own authoritative state
- accept command/control requests
- validate commands
- run tick-based state transitions
- publish state snapshots
- emit event logs and AAR artifacts
- manage session lifecycle and resilience events

Suggested internal modules:
- `net/` — TCP/UDP transport boundaries
- `session/` — connection/session tracking
- `simulation/` — tick loop and state transitions
- `domain/` — entities, commands, judgments
- `logging/` — event emission
- `aar/` — replay/AAR generation
- `config/` — loading/validation of runtime configuration
- `runtime/` — orchestration that binds config, simulation, logs, and artifact paths

Current committed anchor points:
- shared protocol schema: `common/include/icss/protocol/messages.hpp`
- payload structs/serialization: `common/include/icss/protocol/payloads.hpp`, `common/include/icss/protocol/serialization.hpp`, `common/src/serialization.cpp`
- runtime config loader: `common/include/icss/core/config.hpp`, `common/src/config.cpp`
- reusable runtime orchestration: `common/include/icss/core/runtime.hpp`, `common/src/runtime.cpp`
- simulation API: `common/include/icss/core/simulation.hpp`
- simulation runtime: `common/src/simulation.cpp`
- ASCII tactical viewer renderer: `common/src/ascii_tactical_view.cpp`
- server baseline entrypoint: `server/src/main.cpp`
- command console baseline entrypoint: `clients/command-console/src/main.cpp`
- tactical viewer baseline entrypoint: `clients/tactical-viewer/src/main.cpp`

### 2. Command Console Client
Responsibilities:
- create session / join session
- issue operator commands
- display command acknowledgements and critical events
- show connection health and important judgment outcomes

### 3. Minimal 2D Tactical Viewer
Responsibilities:
- render target / asset positions
- show tracking state and snapshot freshness
- display connection status and telemetry
- surface event log panel
- show AAR playback cursor/state during replay

## Authority Model

- The server is the **single source of truth**.
- Clients send requests and render state.
- Final validation and judgment happen only on the server.
- Replay/AAR is generated from server-side event history, not reconstructed from client guesses.

## Tick Model

Each tick should conceptually do:
1. collect inbound commands/events
2. validate and enqueue accepted commands
3. apply state transitions
4. emit important events
5. publish snapshot/telemetry
6. persist replay/AAR-relevant records

The current baseline uses a **deterministic in-process clock** so that generated AAR/example artifacts and regression tests remain stable across runs.

## Boundary Rules

### In Scope for the 4-week baseline
- one scenario
- one authoritative state model
- one event schema
- one resilience path fully evidenced

### Out of Scope for the 4-week baseline
- sophisticated physics
- tactical AI sophistication
- advanced rendering pipeline
- large plugin-style extensibility

## Evidence of Good Architecture

A collaborator should be able to answer these questions quickly:
- Why is the server authoritative?
- Why are TCP and UDP split?
- Where is replay/AAR generated from?
- How does reconnect or timeout affect session state?
- What data is shown by the 2D viewer vs what is decided by the server?
