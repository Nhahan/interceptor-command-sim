# Command Console

Role:
- operator-facing command/control client
- submit commands over the reliability-sensitive path
- display acknowledgements and critical outcomes
- drive interceptor-ready and command-issue transitions for the live scenario

Guardrails:
- sends requests; does not own authoritative state
- favors clarity of operator intent over rich UI behavior

Entrypoint:
- `clients/command-console/src/main.cpp`

Current behavior:
- supports the canonical operator command order in `in_process`
- supports a scripted `socket_live` flow over TCP
- prints command acknowledgements plus one AAR response summary; the scripted live flow now ends after AAR because the runtime auto-archives after judgment
