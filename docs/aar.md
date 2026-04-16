# AAR / Replay Design

## Goal

AAR (After Action Review) is one of the main differentiators between a simple demo and a system that looks operable and explainable.

## Minimum Output

For one sample session, reconstruct:
- session start/end
- client join/leave/reconnect events
- target detection/tracking changes
- operator commands
- server validation/judgment results
- important resilience events

## Event Record Shape

Suggested baseline fields:
- `timestamp`
- `tick`
- `session_id`
- `event_type`
- `source`
- `entity_ids`
- `summary`
- `details`

## Replay Expectations

Baseline replay does **not** need a complex cinematic viewer.
It only needs to support:
- timeline progression
- visible cursor/current step
- state/event correlation
- easy explanation of “what happened when and why”

## Tactical Viewer + AAR

During replay mode, the viewer should show:
- AAR playback cursor
- current snapshot time/tick
- visible event panel updates
- enough state visualization to connect events with positions/status

## Evidence Target

The project should be able to show one artifact under `assets/sample-aar/` that a collaborator can inspect without running the full system.
