# Design FAQ

## Why keep the runtime control-oriented?

Because the runtime is centered on command validation, state authority, resilience handling, and traceable post-run analysis rather than interaction-heavy presentation.

## Why server-authoritative?

To keep validation and assessment in one place. The server is the only source of truth.

## Why TCP and UDP both?

That split appears in both the protocol layer and the transport backend architecture. TCP is reserved for reliability-sensitive control flows, while UDP is reserved for fresh snapshot and telemetry flows. The default runtime stays in-process for deterministic verification, and the separate socket-live backend covers bind/listen plus command/snapshot exchange behavior on the network path.

## Why add a transport abstraction before live sockets?

So the runtime can separate command/snapshot intent from the backend that carries it. The in-process backend keeps local verification deterministic, while the socket-live backend exposes the network path without forcing the entire runtime onto sockets by default.

## Why add viewer heartbeat handling?

So transport-level liveness is observable independently from authoritative command/assessment state. A viewer timeout should affect connection/resilience state, not mission outcome.

## Why keep the live backend single-session?

Because the current scope optimizes for clear transport ownership and explainable behavior. One active fire control console and one active tactical display keep the live path easy to reason about and easy to verify.

## Why separate framing from payload serialization?

So the logical schema remains independent from how bytes move over the wire. The same payload can travel in a JSON line or a length-prefixed binary frame without redefining the underlying command structure.

## Why add snapshot batching/filtering?

So a late-joining or slower viewer can catch up quickly when only the latest state matters.

## Why does the protocol still say `track_*` while the GUI says `Track`?

Because the protocol names are stable implementation identifiers, while the GUI wording is tuned for operator legibility. The authoritative runtime still enters the internal `Tracking` phase, but the operator-facing control is a track toggle: `Acquire Track` establishes a pre-launch firing solution and `Drop Track` leaves the interceptor on an `unguided_intercept` launch path.

## Why include Post-Engagement Review?

Because post-run traceability is part of the system contract, not an afterthought.

## Why use a minimal 2D viewer?

To make state propagation and telemetry legible without drifting into interaction-heavy or entertainment-first UX.

## Why show richer state panels?

So the viewer can expose tracker estimate/covariance state, interceptor status, engage order status, assessment state, and replay position without forcing operators to reconstruct system state from raw logs alone.

## Why remove the Stop button from the GUI flow?

Because the current scenario has no meaningful post-command operator action before review. Once assessment is produced, the runtime auto-archives the run and leaves `Review` / `Reset` as the only useful next actions.
