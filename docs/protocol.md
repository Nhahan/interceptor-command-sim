# Protocol Design

## Design Goal

Keep the protocol small, explainable, and aligned with the system's message: **reliable command handling + timely state propagation**.

## Canonical Schema Source

The committed source of truth for the baseline message schema is:

- `common/include/icss/protocol/messages.hpp`
- `common/include/icss/protocol/payloads.hpp`
- `common/include/icss/protocol/serialization.hpp`

Use this document as the explanation layer and the header as the implementation anchor.
If they diverge, fix both in the same change.

## Transport Split

| Transport | Use | Why |
|---|---|---|
| TCP | session control, command submission, acknowledgements, critical judgment events | reliability and ordering matter |
| UDP | state snapshots, telemetry, non-critical frequent updates | freshness matters more than per-packet reliability |

## TCP Message Kinds (`TcpMessageKind`)

| Kind | Purpose |
|---|---|
| `session_create` | create a session |
| `session_join` | join an existing session |
| `session_leave` | leave session / operator disconnect flow |
| `scenario_start` | start the representative scenario |
| `scenario_stop` | stop or finalize scenario execution |
| `track_request` | request target tracking |
| `asset_activate` | activate or ready an asset |
| `command_issue` | submit a command for validation/judgment |
| `command_ack` | acknowledge accepted or processed command flow |
| `judgment_event` | emit critical server-side judgment result |
| `aar_request` | request replay/AAR output |

## UDP Message Kinds (`UdpMessageKind`)

| Kind | Purpose |
|---|---|
| `world_snapshot` | periodic world snapshot |
| `entity_state` | target/asset state summary |
| `tracking_summary` | tracking-specific state summary |
| `telemetry` | tick/latency/packet-loss/last-snapshot telemetry |

## Event Types (`EventType`)

Committed baseline event categories:
- `session_started`
- `session_ended`
- `client_joined`
- `client_left`
- `client_reconnected`
- `track_updated`
- `asset_updated`
- `command_accepted`
- `command_rejected`
- `judgment_produced`
- `resilience_triggered`

## Baseline Shared Structures

Committed baseline headers/records:
- `SessionEnvelope`
- `SnapshotHeader`
- `TelemetrySample`
- `EventRecordHeader`

## Concrete Payload Types

Committed payload structs:
- `SessionCreatePayload`
- `TrackRequestPayload`
- `AssetActivatePayload`
- `CommandIssuePayload`
- `JudgmentPayload`
- `SnapshotPayload`
- `TelemetryPayload`
- `AarRequestPayload`

## Baseline Serialization Format

The current baseline uses a **deterministic textual wire preview format** for payload serialization.

Format rules:
- `key=value` pairs
- `;` as field separator
- simple scalar-only baseline values
- intended for schema verification and documentation alignment, not final production wire format

Example:

```text
kind=command_issue;session_id=1001;sender_id=101;sequence=7;asset_id=asset-interceptor;target_id=target-alpha
```

## Baseline Validation Behavior

The authoritative runtime now records rejected command attempts as `command_rejected` events.
This matters because command validation is part of the system boundary, not just a UI concern.

## Baseline Reliability Strategy

### TCP
- strict request/response or event acknowledgement semantics for critical operations

### UDP
- latest snapshot wins
- sequence number and snapshot timestamp included
- no complex retransmission requirement in baseline
- recovery target: converge on the next valid snapshot

## Resilience Path to Implement

At least one of the following must be fully demonstrated:
- reconnect + resync
- timeout handling
- UDP snapshot loss convergence

## Packet/Message Design Guidelines

- include message type explicitly
- include session identifier
- include monotonic sequence/timestamp where useful
- keep operator intent separate from rendered world state
- make event payloads AAR-friendly

## Explanation Targets

Be able to explain:
1. why not everything is TCP
2. why the viewer does not own truth
3. why snapshot convergence is enough for the baseline
4. how protocol structure helps replay/AAR
