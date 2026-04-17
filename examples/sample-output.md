# Sample Output

- backend: in_process
- session_id: 1001
- cursor_index: 11/12
- latest_freshness: fresh
- latest_snapshot_sequence: 8
- resilience_case: reconnect_and_resync,udp_snapshot_gap_convergence

```text
=== Tactical Viewer ===
............
............
........A...
............
............
............
....T.......
............
Entities:
- target=target-alpha @ (4, 6) active=yes
- asset=asset-interceptor @ (8, 2) active=yes
State:
- tracking=on (confidence=82%), asset_status=complete, command_status=completed, judgment=intercept_success
Telemetry:
- connection=connected, freshness=fresh, snapshot_sequence=8, tick=3, latency_ms=43, packet_loss_pct=0.0, last_snapshot_ms=1776327004000
AAR:
- cursor_index=11/12
Recent events:
- [tick 1] Command accepted (command_accepted)
- [tick 2] Snapshot gap exercised (resilience_triggered)
- [tick 3] Judgment produced (judgment_produced)
- [tick 3] Session archived (session_ended)
```
