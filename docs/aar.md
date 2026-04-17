# AAR / Replay Design

## Goal

AAR (After Action Review) provides an inspectable record of what happened, when it happened, and why the system reached a given outcome.

## Minimum Output

For one sample session, capture:
- session start/end
- client join/leave/reconnect events
- target detection/tracking changes
- operator commands
- server validation/judgment results
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

## Tactical Viewer + AAR

During replay mode, the viewer should show:
- AAR playback cursor
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
