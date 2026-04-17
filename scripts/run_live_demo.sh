#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
runtime_root="${repo_root}"
host="127.0.0.1"
tcp_port="4000"
udp_port="4001"
frame_format="binary"
tick_sleep_ms="20"
viewer_duration_ms=""
headless="false"
scripted="false"
font_path=""

usage() {
  cat <<'EOF'
usage: ./scripts/run_live_demo.sh [options]

Starts the live server and GUI tactical viewer with one command.
In headless mode it also runs the scripted command console flow automatically.

Options:
  --runtime-root PATH       Runtime/artifact root (default: repo root)
  --host HOST               Host for server/viewer/console (default: 127.0.0.1)
  --tcp-port PORT           TCP port for server/console (default: 4000, use 0 for dynamic)
  --udp-port PORT           UDP port for server/viewer (default: 4001, use 0 for dynamic)
  --frame-format FORMAT     TCP frame format: binary|json (default: binary)
  --tick-sleep-ms MS        Server tick sleep in ms (default: 20)
  --viewer-duration-ms MS   Auto-close viewer after MS; default is 8000 in headless mode and unlimited otherwise
  --font PATH               Override GUI viewer font path
  --scripted                Also run the scripted command console flow in visible mode
  --headless                Run viewer with SDL dummy driver and dump state instead of opening a visible window
  --help                    Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --runtime-root) runtime_root="$2"; shift 2 ;;
    --host) host="$2"; shift 2 ;;
    --tcp-port) tcp_port="$2"; shift 2 ;;
    --udp-port) udp_port="$2"; shift 2 ;;
    --frame-format) frame_format="$2"; shift 2 ;;
    --tick-sleep-ms) tick_sleep_ms="$2"; shift 2 ;;
    --viewer-duration-ms) viewer_duration_ms="$2"; shift 2 ;;
    --font) font_path="$2"; shift 2 ;;
    --scripted) scripted="true"; shift ;;
    --headless) headless="true"; shift ;;
    --help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ ! -x "${repo_root}/build/icss_server" ]]; then
  echo "missing build/icss_server; run cmake configure/build first" >&2
  exit 1
fi
if [[ ! -x "${repo_root}/build/icss_command_console" ]]; then
  echo "missing build/icss_command_console; run cmake configure/build first" >&2
  exit 1
fi
if [[ ! -x "${repo_root}/build/icss_tactical_viewer_gui" ]]; then
  echo "missing build/icss_tactical_viewer_gui; run cmake configure/build first" >&2
  exit 1
fi
if [[ ! -x "${repo_root}/build/icss_artifact_summary" ]]; then
  echo "missing build/icss_artifact_summary; run cmake configure/build first" >&2
  exit 1
fi

if [[ -z "${viewer_duration_ms}" ]]; then
  if [[ "${headless}" == "true" ]]; then
    viewer_duration_ms="1500"
  else
    viewer_duration_ms="0"
  fi
fi

if [[ "${headless}" == "true" ]]; then
  scripted="true"
fi

tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/icss-live-demo.XXXXXX")"
server_log="${tmp_dir}/server.log"
viewer_log="${tmp_dir}/viewer.log"
console_log="${tmp_dir}/console.log"
viewer_dump="${tmp_dir}/viewer-state.json"
server_pid=""
viewer_pid=""

cleanup() {
  local status=$?
  if [[ -n "${viewer_pid}" ]] && kill -0 "${viewer_pid}" 2>/dev/null; then
    kill "${viewer_pid}" 2>/dev/null || true
    wait "${viewer_pid}" 2>/dev/null || true
  fi
  if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  if [[ "${status}" -ne 0 ]]; then
    echo "server_log=${server_log}" >&2
    echo "viewer_log=${viewer_log}" >&2
    echo "console_log=${console_log}" >&2
  fi
  rm -rf "${tmp_dir}"
  exit "${status}"
}
trap cleanup EXIT INT TERM

mkdir -p "${runtime_root}"

"${repo_root}/build/icss_server" \
  --backend socket_live \
  --repo-root "${runtime_root}" \
  --bind-host "${host}" \
  --tcp-port "${tcp_port}" \
  --udp-port "${udp_port}" \
  --run-forever \
  --tick-sleep-ms "${tick_sleep_ms}" \
  --tcp-frame-format "${frame_format}" \
  >"${server_log}" 2>&1 &
server_pid=$!

actual_tcp_port=""
actual_udp_port=""
startup_ready="false"
for _ in $(seq 1 200); do
  if grep -q '^startup_ready=true$' "${server_log}" 2>/dev/null; then
    startup_ready="true"
    server_line="$(grep 'backend=socket_live' "${server_log}" | tail -n 1 || true)"
    actual_tcp_port="$(echo "${server_line}" | sed -n 's/.*tcp_port=\([0-9][0-9]*\).*/\1/p')"
    actual_udp_port="$(echo "${server_line}" | sed -n 's/.*udp_port=\([0-9][0-9]*\).*/\1/p')"
    break
  fi
  sleep 0.05
done

if [[ "${startup_ready}" != "true" || -z "${actual_tcp_port}" || -z "${actual_udp_port}" ]]; then
  echo "server startup failed" >&2
  cat "${server_log}" >&2 || true
  exit 1
fi

viewer_args=(
  "${repo_root}/build/icss_tactical_viewer_gui"
  --host "${host}"
  --udp-port "${actual_udp_port}"
  --tcp-port "${actual_tcp_port}"
  --tcp-frame-format "${frame_format}"
  --duration-ms "${viewer_duration_ms}"
)
if [[ -n "${font_path}" ]]; then
  viewer_args+=(--font "${font_path}")
fi
if [[ "${headless}" == "true" ]]; then
  viewer_args+=(--headless --hidden --dump-state "${viewer_dump}")
fi

"${viewer_args[@]}" >"${viewer_log}" 2>&1 &
viewer_pid=$!

sleep 0.20

if [[ "${scripted}" == "true" ]]; then
  "${repo_root}/build/icss_command_console" \
    --backend socket_live \
    --host "${host}" \
    --tcp-port "${actual_tcp_port}" \
    --tcp-frame-format "${frame_format}" \
    | tee "${console_log}"
else
  echo "interactive_mode=true"
  echo "use the GUI control panel buttons: Start -> Track -> Activate -> Command -> Stop"
  echo "request Review after judgment/archive if you want server-side AAR data"
fi

if [[ "${viewer_duration_ms}" == "0" && "${headless}" != "true" ]]; then
  echo "viewer_running=true"
  echo "close the GUI window to finish the demo"
fi

wait "${viewer_pid}"
viewer_pid=""

kill "${server_pid}" 2>/dev/null || true
wait "${server_pid}" 2>/dev/null || true
server_pid=""

echo "==== artifact summary ===="
"${repo_root}/build/icss_artifact_summary" --repo-root "${runtime_root}"

if [[ -f "${viewer_dump}" ]]; then
  if grep -q '"received_snapshot": true' "${viewer_dump}"; then
    echo "viewer_state.received_snapshot=true"
  fi
  if grep -q '"received_telemetry": true' "${viewer_dump}"; then
    echo "viewer_state.received_telemetry=true"
  fi
fi

echo "demo_server.tcp_port=${actual_tcp_port}"
echo "demo_server.udp_port=${actual_udp_port}"
echo "demo_completed=true"
