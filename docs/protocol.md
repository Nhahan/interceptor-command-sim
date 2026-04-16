# Protocol Design

## Goal

Keep the protocol compact, explicit, and aligned with the runtime boundary: **reliable command handling + timely state propagation**.

## Canonical Schema Source

The source of truth for the message schema is:

- `common/include/icss/protocol/messages.hpp`
- `common/include/icss/protocol/payloads.hpp`
- `common/include/icss/protocol/serialization.hpp`
- `common/include/icss/net/transport.hpp`

Use this document as the explanation layer and the headers as the implementation anchor.
If they diverge, update both in the same change.

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
| `aar_response` | return replay/AAR summary metadata over TCP |

## UDP Message Kinds (`UdpMessageKind`)

| Kind | Purpose |
|---|---|
| `world_snapshot` | periodic world snapshot |
| `entity_state` | target/asset state summary |
| `tracking_summary` | tracking-specific state summary |
| `telemetry` | tick/latency/packet-loss/last-snapshot telemetry |
| `viewer_heartbeat` | viewer liveness heartbeat over UDP |

## Event Types (`EventType`)

Current event categories:
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

## Shared Structures

Current headers/records:
- `SessionEnvelope`
- `SnapshotHeader`
- `TelemetrySample`
- `EventRecordHeader`

## Payload Types

Current payload structs:
- `SessionCreatePayload`
- `SessionJoinPayload`
- `ScenarioStartPayload`
- `ScenarioStopPayload`
- `TrackRequestPayload`
- `AssetActivatePayload`
- `CommandIssuePayload`
- `JudgmentPayload`
- `CommandAckPayload`
- `AarResponsePayload`
- `SnapshotPayload`
- `TelemetryPayload`
- `ViewerHeartbeatPayload`
- `AarRequestPayload`

`SnapshotPayload` carries richer state fields for:
- track confidence
- asset status
- command lifecycle status
- judgment readiness/code

## Serialization Format

The current implementation uses a **deterministic textual wire preview format** for payload serialization.

Format rules:
- `key=value` pairs
- `;` as field separator
- simple scalar-only values
- intended for schema verification and documentation alignment, not final production wire format

Example:

```text
kind=command_issue;session_id=1001;sender_id=101;sequence=7;asset_id=asset-interceptor;target_id=target-alpha
```

## Framing Layer

The implementation separates payload serialization from transport framing:
- `JsonLine` framing for JSON envelope lines
- `Binary` framing for length-prefixed transport frames

The live TCP path exercises binary framing in regression coverage. Framing stays transport-selectable.

## Transport Abstraction

The implementation separates runtime behavior from transport backend selection through `TransportBackend`.

Backend kinds:
- `in_process` — active backend used by the runtime and client previews
- `socket_live` — bind/listen-capable backend for the live transport direction, kept separate from the deterministic in-process runtime

Current live backend scope:
- accepts a TCP command connection
- receives UDP viewer registration datagrams
- processes framed command payloads over TCP
- emits UDP snapshot/telemetry datagrams to the registered viewer endpoint
- handles `scenario_stop` and `aar_request` over TCP
- tracks viewer heartbeat datagrams and raises timeout visibility when the heartbeat window expires
- applies batching/filtering controls when multiple snapshots are pending for a late-joining viewer

## Validation Behavior

The authoritative runtime records rejected command attempts as `command_rejected` events. Validation is part of the system boundary, not a UI detail.

## Reliability Strategy

### TCP
- strict request/response or event acknowledgement semantics for critical operations

### UDP
- latest snapshot wins
- sequence number and snapshot timestamp included
- no complex retransmission requirement in the current scope
- recovery target: converge on the next valid snapshot

## Resilience Path

At least one of the following must be fully exercised:
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
3. why snapshot convergence is sufficient for the current scope
4. how protocol structure helps replay/AAR
