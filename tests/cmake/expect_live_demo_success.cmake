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
        "command_issue: accepted"
        "summary.judgment_code=intercept_success"
        "viewer_state.received_snapshot=true"
        "viewer_state.received_telemetry=true"
        "demo_completed=true")
    string(FIND "${combined_output}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "expected output fragment not found: ${expected}")
    endif()
endforeach()
