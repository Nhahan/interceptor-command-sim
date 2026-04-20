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
skip_build="false"
preserve_existing="false"
regen_samples="false"
sample_mode="all"

actual_tcp_port=""
actual_udp_port=""
tmp_dir=""
server_pid=""
viewer_pid=""

usage() {
  cat <<'EOF'
usage: ./scripts/run_live_demo.sh [options]

Starts the live server and GUI tactical display with one command.
In headless mode it also runs the scripted fire control console flow automatically.

Options:
  --runtime-root PATH       Runtime/artifact root (default: repo root)
  --host HOST               Host for server/viewer/console (default: 127.0.0.1)
  --tcp-port PORT           TCP port for server/console (default: 4000, use 0 for dynamic)
  --udp-port PORT           UDP port for server/viewer (default: 4001, use 0 for dynamic)
  --frame-format FORMAT     TCP frame format: binary|json (default: binary)
  --tick-sleep-ms MS        Server tick sleep in ms (default: 20)
  --viewer-duration-ms MS   Auto-close viewer after MS; default is 8000 in headless mode and unlimited otherwise
  --font PATH               Override GUI viewer font path
  --skip-build              Do not configure/build before launching the demo
  --preserve-existing       Do not kill repo-local server/viewer/console processes before launching
  --scripted                Also run the scripted fire control console flow in visible mode
  --headless                Run viewer with SDL dummy driver and dump state instead of opening a visible window
  --regen-samples           Regenerate tracked_intercept/unguided_intercept sample artifacts, viewer-state goldens, and screenshots under the runtime root
  --sample-mode MODE        tracked_intercept|unguided_intercept|all for --regen-samples (default: all)
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
    --skip-build) skip_build="true"; shift ;;
    --preserve-existing) preserve_existing="true"; shift ;;
    --scripted) scripted="true"; shift ;;
    --headless) headless="true"; shift ;;
    --regen-samples) regen_samples="true"; shift ;;
    --sample-mode) sample_mode="$2"; shift 2 ;;
    --help) usage; exit 0 ;;
    *) echo "unknown argument: $1" >&2; usage; exit 1 ;;
  esac
done

case "${sample_mode}" in
  tracked_intercept|unguided_intercept|all) ;;
  *) echo "unsupported sample mode: ${sample_mode}" >&2; exit 1 ;;
esac

ensure_latest_binaries() {
  if [[ ! -f "${repo_root}/build/CMakeCache.txt" ]]; then
    cmake -S "${repo_root}" -B "${repo_root}/build" -DCMAKE_BUILD_TYPE=Debug
  fi
  cmake --build "${repo_root}/build" \
    --target icss_server icss_tactical_display_gui icss_fire_control_console icss_artifact_summary
}

stop_existing_demo_processes() {
  pkill -f "${repo_root}/build/icss_server" 2>/dev/null || true
  pkill -f "${repo_root}/build/icss_tactical_display_gui" 2>/dev/null || true
  pkill -f "${repo_root}/build/icss_fire_control_console" 2>/dev/null || true
  sleep 0.2
}

ensure_runtime_configs() {
  local target_root="$1"
  mkdir -p "${target_root}/configs"
  for name in server scenario logging; do
    local source_path="${repo_root}/configs/${name}.example.yaml"
    local target_path="${target_root}/configs/${name}.example.yaml"
    if [[ ! -f "${target_path}" ]]; then
      cp "${source_path}" "${target_path}"
    fi
  done
}

copy_runtime_configs() {
  local source_root="$1"
  local target_root="$2"
  rm -rf "${target_root}"
  mkdir -p "${target_root}/configs"
  cp "${source_root}/configs/server.example.yaml" "${target_root}/configs/server.example.yaml"
  cp "${source_root}/configs/scenario.example.yaml" "${target_root}/configs/scenario.example.yaml"
  cp "${source_root}/configs/logging.example.yaml" "${target_root}/configs/logging.example.yaml"
}

start_live_server() {
  local live_root="$1"
  local server_log="$2"
  local live_tcp_port="$3"
  local live_udp_port="$4"

  actual_tcp_port=""
  actual_udp_port=""
  local startup_ready="false"

  "${repo_root}/build/icss_server" \
    --backend socket_live \
    --repo-root "${live_root}" \
    --bind-host "${host}" \
    --tcp-port "${live_tcp_port}" \
    --udp-port "${live_udp_port}" \
    --run-forever \
    --tick-sleep-ms "${tick_sleep_ms}" \
    --tcp-frame-format "${frame_format}" \
    >"${server_log}" 2>&1 &
  server_pid=$!

  for _ in $(seq 1 200); do
    if grep -q '^startup_ready=true$' "${server_log}" 2>/dev/null; then
      startup_ready="true"
      local server_line
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
}

stop_live_server() {
  if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
  fi
  server_pid=""
}

sanitize_screenshot() {
  local screenshot_path="$1"
  if ! command -v magick >/dev/null 2>&1; then
    return
  fi
  local work_dir
  work_dir="$(mktemp -d "${TMPDIR:-/tmp}/icss-screenshot-sanitize.XXXXXX")"
  magick -size 1360x860 xc:"#181a20" "${work_dir}/base.bmp"
  magick "${screenshot_path}" -crop 1312x64+24+18 +repage "${work_dir}/header.bmp"
  magick "${screenshot_path}" -crop 330x233+557+106 +repage "${work_dir}/phase.bmp"
  magick "${screenshot_path}" -crop 331x233+863+106 +repage "${work_dir}/decision.bmp"
  magick "${screenshot_path}" -crop 331x233+863+347 +repage "${work_dir}/control.bmp"
  magick "${screenshot_path}" -crop 331x150+863+598 +repage "${work_dir}/setup.bmp"
  magick "${work_dir}/base.bmp" \
    "${work_dir}/header.bmp" -geometry +24+18 -composite \
    "${work_dir}/phase.bmp" -geometry +557+106 -composite \
    "${work_dir}/decision.bmp" -geometry +863+106 -composite \
    "${work_dir}/control.bmp" -geometry +863+347 -composite \
    "${work_dir}/setup.bmp" -geometry +863+598 -composite \
    "${screenshot_path}"
  rm -rf "${work_dir}"
}

run_headless_viewer() {
  local viewer_repo_root="$1"
  local dump_state_path="$2"
  local dump_golden_state_path="$3"
  local dump_frame_path="$4"
  local auto_controls="$5"
  local duration_ms="$6"
  local viewer_log="$7"

  local viewer_args=(
    "${repo_root}/build/icss_tactical_display_gui"
    --host "${host}"
    --udp-port "${actual_udp_port}"
    --tcp-port "${actual_tcp_port}"
    --tcp-frame-format "${frame_format}"
    --heartbeat-interval-ms "100"
    --repo-root "${viewer_repo_root}"
    --duration-ms "${duration_ms}"
    --headless
    --hidden
    --dump-state "${dump_state_path}"
    --dump-golden-state "${dump_golden_state_path}"
    --dump-frame "${dump_frame_path}"
    --auto-control-script
    --auto-controls "${auto_controls}"
  )
  if [[ -n "${font_path}" ]]; then
    viewer_args+=(--font "${font_path}")
  fi

  SDL_VIDEODRIVER=dummy "${viewer_args[@]}" >"${viewer_log}" 2>&1 &
  viewer_pid=$!
  wait "${viewer_pid}"
  viewer_pid=""
  sanitize_screenshot "${dump_frame_path}"
}

copy_tracked_intercept_bundle() {
  local sample_root="$1"
  mkdir -p "${runtime_root}/assets/sample-aar" "${runtime_root}/examples" "${runtime_root}/logs"
  rm -f "${runtime_root}/assets/sample-aar/replay-timeline.json"
  rm -f "${runtime_root}/assets/sample-aar/session-summary.json"
  rm -f "${runtime_root}/assets/sample-aar/session-summary.md"
  cp "${sample_root}/assets/sample-aar/replay-timeline.json" "${runtime_root}/assets/sample-aar/replay-timeline.json"
  cp "${sample_root}/assets/sample-aar/session-summary.json" "${runtime_root}/assets/sample-aar/session-summary.json"
  cp "${sample_root}/assets/sample-aar/session-summary.md" "${runtime_root}/assets/sample-aar/session-summary.md"
  cp "${sample_root}/examples/sample-output.md" "${runtime_root}/examples/sample-output.md"
  cp "${sample_root}/logs/session.log" "${runtime_root}/logs/session.log"
}

copy_unguided_intercept_bundle() {
  local sample_root="$1"
  mkdir -p "${runtime_root}/assets/sample-aar" "${runtime_root}/examples" "${runtime_root}/logs"
  rm -rf "${runtime_root}/assets/sample-aar/unguided_intercept"
  mkdir -p "${runtime_root}/assets/sample-aar/unguided_intercept"
  cp "${sample_root}/assets/sample-aar/replay-timeline.json" "${runtime_root}/assets/sample-aar/unguided_intercept/replay-timeline.json"
  cp "${sample_root}/assets/sample-aar/session-summary.json" "${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.json"
  cp "${sample_root}/assets/sample-aar/session-summary.md" "${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.md"
  cp "${sample_root}/examples/sample-output.md" "${runtime_root}/examples/sample-output-unguided_intercept.md"
  cp "${sample_root}/logs/session.log" "${runtime_root}/logs/session-unguided_intercept.log"
}

capture_gui_sample() {
  local capture_runtime_root="$1"
  local golden_state_path="$2"
  local screenshot_path="$3"
  local auto_controls="$4"
  local duration_ms="$5"
  local capture_name="$6"

  local capture_dir="${tmp_dir}/${capture_name}"
  local live_root="${capture_dir}/runtime-root"
  local server_log="${capture_dir}/server.log"
  local viewer_log="${capture_dir}/viewer.log"
  local viewer_dump="${capture_dir}/viewer-state.json"
  local viewer_golden_dump="${capture_dir}/viewer-golden-state.json"
  mkdir -p "${capture_dir}"
  mkdir -p "$(dirname "${golden_state_path}")"
  mkdir -p "$(dirname "${screenshot_path}")"
  copy_runtime_configs "${capture_runtime_root}" "${live_root}"

  start_live_server "${live_root}" "${server_log}" 0 0
  run_headless_viewer "${live_root}" "${viewer_dump}" "${viewer_golden_dump}" "${screenshot_path}" "${auto_controls}" "${duration_ms}" "${viewer_log}"
  stop_live_server

  grep -q '"received_snapshot": true' "${viewer_dump}"
  grep -q '"received_telemetry": true' "${viewer_dump}"
  cp "${viewer_golden_dump}" "${golden_state_path}"
}

run_inprocess_sample() {
  local sample_root="$1"
  local mode="$2"
  local log_path="$3"
  "${repo_root}/build/icss_server" \
    --backend in_process \
    --repo-root "${sample_root}" \
    --sample-mode "${mode}" \
    >"${log_path}" 2>&1
}

regen_samples_mode() {
  local tracked_intercept_root="${tmp_dir}/tracked_intercept-runtime"
  local unguided_intercept_root="${tmp_dir}/unguided_intercept-runtime"
  local tracked_intercept_log="${tmp_dir}/tracked_intercept-baseline.log"
  local unguided_intercept_log="${tmp_dir}/unguided_intercept-baseline.log"

  ensure_runtime_configs "${runtime_root}"
  copy_runtime_configs "${runtime_root}" "${tracked_intercept_root}"
  copy_runtime_configs "${runtime_root}" "${unguided_intercept_root}"
  mkdir -p "${runtime_root}/assets/screenshots"

  local need_tracked_intercept="false"
  local need_unguided_intercept="false"
  if [[ "${sample_mode}" == "tracked_intercept" ]]; then
    need_tracked_intercept="true"
  elif [[ "${sample_mode}" == "unguided_intercept" ]]; then
    need_tracked_intercept="true"
    need_unguided_intercept="true"
  else
    need_tracked_intercept="true"
    need_unguided_intercept="true"
  fi

  if [[ "${need_tracked_intercept}" == "true" ]]; then
    run_inprocess_sample "${tracked_intercept_root}" tracked_intercept "${tracked_intercept_log}"
  fi
  if [[ "${sample_mode}" == "tracked_intercept" || "${sample_mode}" == "all" ]]; then
    copy_tracked_intercept_bundle "${tracked_intercept_root}"
    capture_gui_sample "${runtime_root}" \
      "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept-state.json" \
      "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept.bmp" \
      "Start,Track,Ready,Engage" \
      "2400" \
      "tracked_intercept-capture"
  fi

  if [[ "${need_unguided_intercept}" == "true" ]]; then
    run_inprocess_sample "${unguided_intercept_root}" unguided_intercept "${unguided_intercept_log}"
  fi
  if [[ "${sample_mode}" == "unguided_intercept" || "${sample_mode}" == "all" ]]; then
    copy_unguided_intercept_bundle "${unguided_intercept_root}"
    capture_gui_sample "${runtime_root}" \
      "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept-state.json" \
      "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept.bmp" \
      "Start,Track,Ready,Track,Engage" \
      "3200" \
      "unguided_intercept-capture"
  fi

  if [[ "${sample_mode}" == "tracked_intercept" ]]; then
    echo "==== tracked_intercept artifact summary ===="
    "${repo_root}/build/icss_artifact_summary" --repo-root "${tracked_intercept_root}"
  else
    echo "==== unguided_intercept artifact summary ===="
    "${repo_root}/build/icss_artifact_summary" \
      --tracked_intercept-root "${tracked_intercept_root}" \
      --unguided_intercept-root "${unguided_intercept_root}"
  fi
  if [[ "${sample_mode}" == "tracked_intercept" || "${sample_mode}" == "all" ]]; then
    echo "tracked_intercept_sample.summary=${runtime_root}/assets/sample-aar/session-summary.md"
    echo "tracked_intercept_sample.output=${runtime_root}/examples/sample-output.md"
    echo "tracked_intercept_sample.golden_state=${runtime_root}/assets/screenshots/tactical-display-tracked_intercept-state.json"
    echo "tracked_intercept_sample.screenshot=${runtime_root}/assets/screenshots/tactical-display-tracked_intercept.bmp"
  fi
  if [[ "${sample_mode}" == "unguided_intercept" || "${sample_mode}" == "all" ]]; then
    echo "unguided_intercept_sample.summary=${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.md"
    echo "unguided_intercept_sample.output=${runtime_root}/examples/sample-output-unguided_intercept.md"
    echo "unguided_intercept_sample.golden_state=${runtime_root}/assets/screenshots/tactical-display-unguided_intercept-state.json"
    echo "unguided_intercept_sample.screenshot=${runtime_root}/assets/screenshots/tactical-display-unguided_intercept.bmp"
  fi
  echo "regen_completed=true"
}

if [[ "${skip_build}" != "true" ]]; then
  ensure_latest_binaries
fi

if [[ "${preserve_existing}" != "true" ]]; then
  stop_existing_demo_processes
fi

if [[ ! -x "${repo_root}/build/icss_server" || ! -x "${repo_root}/build/icss_fire_control_console" || ! -x "${repo_root}/build/icss_tactical_display_gui" || ! -x "${repo_root}/build/icss_artifact_summary" ]]; then
  echo "required demo binaries are missing after build" >&2
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
ensure_runtime_configs "${runtime_root}"

if [[ "${regen_samples}" == "true" ]]; then
  regen_samples_mode
  exit 0
fi

start_live_server "${runtime_root}" "${server_log}" "${tcp_port}" "${udp_port}"

viewer_args=(
  "${repo_root}/build/icss_tactical_display_gui"
  --host "${host}"
  --udp-port "${actual_udp_port}"
  --tcp-port "${actual_tcp_port}"
  --tcp-frame-format "${frame_format}"
  --heartbeat-interval-ms "100"
  --repo-root "${runtime_root}"
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
  "${repo_root}/build/icss_fire_control_console" \
    --backend socket_live \
    --host "${host}" \
    --tcp-port "${actual_tcp_port}" \
    --tcp-frame-format "${frame_format}" \
    --repo-root "${runtime_root}" \
    | tee "${console_log}"
else
  echo "interactive_mode=true"
  echo "use the GUI control panel buttons: Start -> Track -> Ready -> Engage"
  echo "request AAR after assessment/archive if you want server-side AAR data"
fi

if [[ "${viewer_duration_ms}" == "0" && "${headless}" != "true" ]]; then
  echo "viewer_running=true"
  echo "close the GUI window to finish the demo"
fi

wait "${viewer_pid}"
viewer_pid=""

stop_live_server

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
