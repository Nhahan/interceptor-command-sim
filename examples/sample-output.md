# Sample Output

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
- connection=connected, tick=3, latency_ms=43, packet_loss=0, last_snapshot_ms=1776327004000
AAR:
- cursor_index=11/12
Recent events:
- [tick 1] Command accepted (command_accepted)
- [tick 2] Snapshot gap exercised (resilience_triggered)
- [tick 3] Judgment produced (judgment_produced)
- [tick 3] Session archived (session_ended)
```
