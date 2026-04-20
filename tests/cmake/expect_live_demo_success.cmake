if(NOT DEFINED DEMO_SCRIPT)
    message(FATAL_ERROR "DEMO_SCRIPT is required")
endif()

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif()

set(runtime_root "${CMAKE_CURRENT_BINARY_DIR}/live-demo-script-smoke")
file(REMOVE_RECURSE "${runtime_root}")
file(MAKE_DIRECTORY "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/server.example.yaml" DESTINATION "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/scenario.example.yaml" DESTINATION "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/logging.example.yaml" DESTINATION "${runtime_root}/configs")
file(WRITE "${runtime_root}/configs/scenario.example.yaml" "scenario:\n  name: basic_intercept_training\n  description: live demo timeout verification\nentities:\n  targets: 1\n  assets: 1\nrules:\n  enable_replay: true\n  telemetry_interval_ms: 200\n  world_width: 576\n  world_height: 384\n  target_start_x: 80\n  target_start_y: 300\n  target_velocity_x: 9\n  target_velocity_y: -6\n  interceptor_start_x: 0\n  interceptor_start_y: 0\n  interceptor_speed_per_tick: 8\n  intercept_radius: 12\n  engagement_timeout_ticks: 10\n  seeker_fov_deg: 45\n  launch_angle_deg: 45\n")

execute_process(
    COMMAND /bin/bash "${DEMO_SCRIPT}"
        --runtime-root "${runtime_root}"
        --headless
        --viewer-duration-ms 1200
        --tcp-port 0
        --udp-port 0
        --frame-format binary
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE)

set(combined_output "${output}")
if(NOT error_output STREQUAL "")
    if(NOT combined_output STREQUAL "")
        string(APPEND combined_output "\n")
    endif()
    string(APPEND combined_output "${error_output}")
endif()

message("${combined_output}")

if(NOT result EQUAL 0)
    message(FATAL_ERROR "expected live demo script to succeed")
endif()

foreach(expected
        "engage_order: accepted"
        "tracked_intercept.summary.assessment_code=timeout_observed"
        "unguided_intercept.available=false"
        "viewer_state.received_snapshot=true"
        "viewer_state.received_telemetry=true"
        "demo_completed=true")
    string(FIND "${combined_output}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "expected output fragment not found: ${expected}")
    endif()
endforeach()
