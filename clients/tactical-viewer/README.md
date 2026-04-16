# Tactical Viewer

Role:
- minimal 2D tactical/state viewer
- render positions/status as an observability-first display
- display telemetry and event panel
- support AAR playback cursor visualization

Required elements:
- target / asset position icons
- tracking status
- tracking confidence
- connection status
- tick / latency / packet loss telemetry
- last snapshot timestamp
- event log panel
- AAR playback cursor
- asset status / command lifecycle / judgment state

Non-goals:
- direct-control interaction loops
- cinematic effects
- score / reward UI

Entrypoint:
- `clients/tactical-viewer/src/main.cpp`

Current behavior:
- renders an ASCII tactical frame
- shows telemetry, connection state, recent events, and AAR cursor position
