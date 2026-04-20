# Tactical Viewer

Role:
- minimal 2D tactical/state viewer
- render positions/status as an observability-first display
- display telemetry and event panel
- support post-engagement review cursor visualization

Required elements:
- target / interceptor position icons
- tracking status
- tracking residual / covariance state
- connection status
- picture status
- tick / update-gap / packet loss telemetry
- snapshot sequence
- last snapshot timestamp
- event log panel
- post-engagement review cursor
- interceptor status / engage order status / assessment state

Non-goals:
- direct-control interaction loops
- cinematic effects
- score / reward UI

Entrypoint:
- `clients/tactical-viewer/src/main.cpp`

Current behavior:
- renders an ASCII tactical frame
- shows link/picture status, connection state, recent events, and post-engagement review cursor position
