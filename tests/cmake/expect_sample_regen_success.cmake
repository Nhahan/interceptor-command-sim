if(NOT DEFINED DEMO_SCRIPT)
    message(FATAL_ERROR "DEMO_SCRIPT is required")
endif()

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif()

set(runtime_root "${CMAKE_CURRENT_BINARY_DIR}/sample-regen-script-smoke")
file(REMOVE_RECURSE "${runtime_root}")
file(MAKE_DIRECTORY "${runtime_root}")

execute_process(
    COMMAND /bin/bash "${DEMO_SCRIPT}"
        --runtime-root "${runtime_root}"
        --regen-samples
        --sample-mode all
        --skip-build
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
    message(FATAL_ERROR "expected sample regen script to succeed")
endif()

foreach(expected
        "guided_sample.summary="
        "guided_sample.golden_state="
        "straight_sample.summary="
        "straight_sample.golden_state="
        "guided_sample.screenshot="
        "straight_sample.screenshot="
        "regen_completed=true")
    string(FIND "${combined_output}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "expected output fragment not found: ${expected}")
    endif()
endforeach()

foreach(path_check
        "${runtime_root}/assets/sample-aar/session-summary.md"
        "${runtime_root}/assets/sample-aar/session-summary.json"
        "${runtime_root}/assets/sample-aar/replay-timeline.json"
        "${runtime_root}/assets/sample-aar/straight/session-summary.md"
        "${runtime_root}/assets/sample-aar/straight/session-summary.json"
        "${runtime_root}/assets/sample-aar/straight/replay-timeline.json"
        "${runtime_root}/examples/sample-output.md"
        "${runtime_root}/examples/sample-output-straight.md"
        "${runtime_root}/assets/screenshots/tactical-viewer-guidance-state.json"
        "${runtime_root}/assets/screenshots/tactical-viewer-straight-state.json"
        "${runtime_root}/assets/screenshots/tactical-viewer-guidance.bmp"
        "${runtime_root}/assets/screenshots/tactical-viewer-straight.bmp")
    if(NOT EXISTS "${path_check}")
        message(FATAL_ERROR "expected regenerated file missing: ${path_check}")
    endif()
endforeach()

file(READ "${runtime_root}/assets/sample-aar/session-summary.md" guided_summary)
string(FIND "${guided_summary}" "judgment_code: intercept_success" guided_summary_index)
if(guided_summary_index EQUAL -1)
    message(FATAL_ERROR "guided summary did not contain intercept_success")
endif()

file(READ "${runtime_root}/assets/sample-aar/straight/session-summary.md" straight_summary)
string(FIND "${straight_summary}" "judgment_code: timeout_observed" straight_summary_index)
if(straight_summary_index EQUAL -1)
    message(FATAL_ERROR "straight summary did not contain timeout_observed")
endif()

file(READ "${runtime_root}/assets/screenshots/tactical-viewer-guidance-state.json" guided_golden_state)
foreach(expected
        "\"schema_version\": \"icss-gui-viewer-golden-state-v1\""
        "\"judgment_code\": \"intercept_success\""
        "\"effective_guidance_state\": \"on\""
        "\"effective_launch_mode\": \"guided\"")
    string(FIND "${guided_golden_state}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "guided golden-state did not contain expected fragment: ${expected}")
    endif()
endforeach()

file(READ "${runtime_root}/assets/screenshots/tactical-viewer-straight-state.json" straight_golden_state)
foreach(expected
        "\"schema_version\": \"icss-gui-viewer-golden-state-v1\""
        "\"judgment_code\": \"timeout_observed\""
        "\"effective_guidance_state\": \"off\""
        "\"effective_launch_mode\": \"straight\"")
    string(FIND "${straight_golden_state}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "straight golden-state did not contain expected fragment: ${expected}")
    endif()
endforeach()

file(SIZE "${runtime_root}/assets/screenshots/tactical-viewer-guidance.bmp" guided_screenshot_size)
file(SIZE "${runtime_root}/assets/screenshots/tactical-viewer-straight.bmp" straight_screenshot_size)
if(guided_screenshot_size LESS 1)
    message(FATAL_ERROR "guided screenshot was empty")
endif()
if(straight_screenshot_size LESS 1)
    message(FATAL_ERROR "straight screenshot was empty")
endif()
