# Sample Output

- schema_version: icss-sample-output-v1
- backend: socket_live
- session_id: 1001
- cursor_index: 0/1
- command_console_connection: disconnected
- viewer_connection: disconnected
- latest_freshness: stale
- latest_snapshot_sequence: 2
- last_event_type: resilience_triggered
- resilience_case: udp_snapshot_gap_convergence

```text
=== Tactical Viewer ===
............
............
........A...
............
............
............
............
.T..........
Entities:
- target=target-alpha @ (1, 7) active=no
- asset=asset-interceptor @ (8, 2) active=no
State:
- tracking=off (confidence=0%), asset_status=idle, command_status=none, judgment=pending
Telemetry:
- connection=disconnected, freshness=stale, snapshot_sequence=2, tick=2, latency_ms=47, packet_loss_pct=25.0, last_snapshot_ms=1776327000450
AAR:
- cursor_index=0/1
Recent events:
- [tick 2] Snapshot gap exercised (resilience_triggered)
```
