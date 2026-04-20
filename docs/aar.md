# Post-Engagement Review / Replay Design

## Goal

Post-engagement review (`AAR`, After Action Review) provides an inspectable record of what happened, when it happened, and why the system reached a given outcome.

## Minimum Output

For one sample session, capture:
- session start/end
- client join/leave/reconnect events
- target detection plus track acquire/drop changes
- operator commands
- server validation/assessment results
- intercept profile and launch angle context
- target deactivation on successful intercept before archive
- important resilience events

## Event Record Shape

Suggested fields:
- `timestamp`
- `tick`
- `session_id`
- `event_type`
- `source`
- `entity_ids`
- `summary`
- `details`

## Replay Expectations

Replay does **not** need a complex cinematic viewer.
It only needs to support:
- timeline progression
- visible cursor/current step
- state/event correlation
- easy explanation of “what happened when and why”

Current control semantics:
- `absolute` jumps to the requested cursor index
- `step_forward` advances from the current cursor
- `step_backward` moves back from the current cursor
- out-of-range requests clamp to the nearest valid cursor

## Tactical Viewer + Post-Engagement Review

During replay mode, the viewer should show:
- post-engagement review cursor
- current snapshot time/tick
- visible event panel updates
- enough state visualization to connect events with positions/status

The live `aar_response` should report:
- applied cursor index
- control used
- requested index
- whether the result was clamped at a boundary

## Output Target

Keep at least one inspectable artifact under `assets/sample-aar/`.
The session summary should expose latest snapshot metadata, picture status, and a short recent-event section.
The session summary should also expose effective track state, intercept profile, and launch angle metadata.
The replay timeline and summary artifacts should carry explicit schema/version metadata.
When both are present, `session-summary.md` is the human-readable view and `session-summary.json` is the machine-readable companion.

The normal GUI flow auto-archives immediately after assessment, so post-engagement review normally begins from an already-archived run rather than waiting for an explicit stop command.
