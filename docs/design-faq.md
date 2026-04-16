# Design FAQ

## Why is this not just a game server?

Because the system is framed around command validation, state authority, resilience, and AAR traceability rather than player action, reward loops, or visual entertainment.

## Why server-authoritative?

To make the validation/judgment path explicit and explainable. The server is the only source of truth.

## Why TCP and UDP both?

In the current baseline, that split is expressed as a committed protocol schema + payload serialization layer. TCP is reserved for reliability-sensitive control flows, while UDP is reserved for fresh snapshot/telemetry flows. Live socket transport is the next implementation layer, not something that is already claimed as shipped.

## Why include AAR?

Because the project should show not just execution, but also explainability and post-run traceability.

## Why use a minimal 2D viewer?

To make state propagation and telemetry legible without drifting into game-like UX.
