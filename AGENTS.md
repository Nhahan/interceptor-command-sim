# AGENTS.md

This file is the root instruction map for agents working in this repository.
Keep it short. Treat `README.md` and `docs/` as the detailed source of truth.

## 1) Project identity

- Project: **Interceptor Command Simulation System**
- Type: **single-system repository**
- Primary focus: **server-side / real-time systems engineering**
- Secondary focus: state management, command handling, observability, and control-system thinking

## 2) Technical objectives

Build a **C++ server-authoritative real-time simulation/control system** focused on command validation, state propagation, observability, and replayability.

The repository provides:
- server-authoritative state management
- TCP/UDP responsibility split
- multi-client command/control + state propagation
- tick-based simulation flow
- replayable logging / AAR
- resilience handling for at least one abnormal network case
- operability and maintainable system structure

## 3) Implementation boundaries

Keep the system compact and do not silently expand scope.

### Core shape
- **1 server**
- **2 clients**
  - command console
  - minimal 2D tactical viewer
- **1 representative scenario**
- server-side validation / judgment
- TCP/UDP split with documented responsibilities
- replayable event logging / AAR
- **at least 1 resilience case**
  - reconnect, timeout, or UDP loss convergence

### Repository contents
- repository source
- documentation
- generated logs and sample outputs

### Do not expand into
- multiple parallel project tracks
- flashy graphics / effects / entertainment-oriented HUD patterns
- direct action controls (WASD, manual aiming)
- ranking / item / progression systems
- precise real-world weapons physics or tactics
- extra scenarios unless the current scope is already complete

## 4) Viewer role

The tactical viewer exists to make UDP/state propagation and AAR legible.
It stays **read-only / observability-first**.

### Required viewer elements
- target / asset position icons
- tracking status
- connection status
- tick / latency / packet loss telemetry
- last snapshot timestamp
- event log panel
- AAR playback cursor

### Forbidden viewer drift
- interaction-heavy control loops
- cinematic effects
- score / reward / progression UI
- direct-control schemes; the viewer remains read-only / observability-first

## 5) Current codebase status

This repository contains an implemented system with deterministic local verification.

Current shape:
- documented runtime, protocol, and operations boundaries
- CMake-based configure/build/test flow
- authoritative runtime under `common/include/icss/core/` and `common/src/`
- shared protocol schema under `common/include/icss/protocol/`
- in-process and socket-based transport backends
- replay, timeout, batching, and logging coverage in tests

Therefore:
- do **not** invent capabilities beyond the current implementation
- when changing build/test tooling, also update `README.md`, `docs/test-report.md`, and this file in the same change

## 6) Read these files first

Read in this order:
1. `README.md`
2. `docs/implementation-checklist.md`
3. `docs/architecture.md`
4. `docs/protocol.md`
5. `docs/scenario.md`
6. `docs/operations.md`
7. `docs/aar.md`
8. `docs/test-report.md`
9. `docs/design-faq.md`

If code and docs diverge, fix the divergence instead of working around it silently.

## 7) Working rules for agents

- Prefer **small, legible, verifiable** changes.
- Preserve the repository's **single-system story** across code, docs, walkthrough material, and AAR artifacts.
- Keep boundaries explicit:
  - server decides truth
  - clients request / render
  - AAR is derived from server-side history
- Prefer repository-local facts over chat history.
- If you add a rule that should persist, encode it in repo docs or tooling rather than relying on conversation memory.
- If a change affects architecture, protocol, scenario flow, logging, resilience, or walkthrough scope, update the matching doc under `docs/`.

## 8) Build / test commands

Canonical commands:

- Configure: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- Build: `cmake --build build`
- Test: `ctest --test-dir build --output-on-failure`

If these commands change, update this file, `README.md`, and `docs/test-report.md` together.

## 9) When to add nested instructions later

If `server/` and `clients/` begin to need materially different rules, add nested `AGENTS.md` files there.
Keep the root file short and let subdirectory files carry local detail.

## 10) Internal-only notes

Use `docs/internal/` for private local notes that should not ship in the public repository.
That directory is intentionally gitignored.
