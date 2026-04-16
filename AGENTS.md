# AGENTS.md

This file is the **root instruction map** for coding agents working in this repository.
It is intentionally concise: use it as a table of contents, not as a giant manual.
The detailed system of record lives in `README.md` and `docs/`.

## 1) Project identity

- Project: **Interceptor Command Simulation System**
- Type: **single focused project**
- Primary focus: **server-side / real-time systems engineering**
- Secondary focus: state management, command handling, observability, and control-system thinking

## 2) What this repository is trying to prove

Build a **C++ server-authoritative real-time simulation/control system** that reads like a serious system project, not a consumer game.

The repository should demonstrate:
- server-authoritative state management
- TCP/UDP responsibility split
- multi-client command/control + state propagation
- tick-based simulation flow
- replayable logging / AAR
- resilience handling for at least one abnormal network case
- operability and explanation quality suitable for design review and maintenance

## 3) Locked scope (do not silently expand)

This repo is locked to a **4-week Standard track**.

### Must implement
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

### Submission target
- **GitHub repository + 3–5 minute demo video**

### Do not expand into
- multiple parallel project tracks
- multiple project tracks
- flashy graphics / effects / game-like HUD
- direct action controls (WASD, manual aiming)
- ranking / item / progression systems
- precise real-world weapons physics or tactics
- extra scenarios unless the baseline is already complete

## 4) The 2D viewer is not a game client

The tactical viewer exists to make UDP/state propagation and AAR legible.
It must stay **read-only / observability-first**.

### Required viewer elements
- target / asset position icons
- tracking status
- connection status
- tick / latency / packet loss telemetry
- last snapshot timestamp
- event log panel
- AAR playback cursor

### Forbidden viewer drift
- action gameplay
- cinematic effects
- score / reward / progression UI
- control schemes that make this look like a player client

## 5) Current repository stage

This repository is in **execution-prep / early implementation** stage.

At this moment:
- documentation scaffold exists
- config examples exist
- a minimal C++/CMake skeleton is committed
- a canonical protocol schema header is committed at `common/include/icss/protocol/messages.hpp`
- a baseline authoritative simulation runtime is committed under `common/include/icss/core/` and `common/src/`
- payload serialization and config loading are committed
- baseline regressions cover protocol, payload codec, scenario flow, invalid command rejection, runtime config loading, resilience, and timeout visibility

Therefore:
- do **not** invent capabilities beyond the committed skeleton
- when changing build/test tooling, also update `README.md`, `docs/test-report.md`, and this file in the same change

## 6) Read these files first

Start here, in order:
1. `README.md`
2. `docs/week1-checklist.md`
3. `docs/architecture.md`
4. `docs/protocol.md`
5. `docs/scenario.md`
6. `docs/operations.md`
7. `docs/aar.md`
8. `docs/test-report.md`
9. `docs/design-faq.md`

Treat those docs as the detailed source of truth.
If code and docs diverge, fix the divergence instead of working around it silently.

## 7) Working rules for agents

- Prefer **small, legible, verifiable** changes.
- Preserve the repository's **single-system story** across code, docs, demo, and AAR artifacts.
- Keep boundaries explicit:
  - server decides truth
  - clients request / render
  - AAR is derived from server-side history
- Prefer repository-local facts over chat history.
- If you add a rule that should persist, encode it in repo docs or tooling rather than relying on conversation memory.
- If a change affects architecture, protocol, scenario flow, logging, resilience, or demo scope, update the matching doc under `docs/`.

## 8) Build / test commands

Canonical commands are now:

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
