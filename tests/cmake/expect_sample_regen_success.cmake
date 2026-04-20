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
        "tracked_intercept_sample.summary="
        "tracked_intercept_sample.golden_state="
        "unguided_intercept_sample.summary="
        "unguided_intercept_sample.golden_state="
        "tracked_intercept_sample.screenshot="
        "unguided_intercept_sample.screenshot="
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
        "${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.md"
        "${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.json"
        "${runtime_root}/assets/sample-aar/unguided_intercept/replay-timeline.json"
        "${runtime_root}/examples/sample-output.md"
        "${runtime_root}/examples/sample-output-unguided_intercept.md"
        "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept-state.json"
        "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept-state.json"
        "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept.bmp"
        "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept.bmp")
    if(NOT EXISTS "${path_check}")
        message(FATAL_ERROR "expected regenerated file missing: ${path_check}")
    endif()
endforeach()

file(READ "${runtime_root}/assets/sample-aar/session-summary.md" tracked_intercept_summary)
string(FIND "${tracked_intercept_summary}" "assessment_code: intercept_success" tracked_intercept_summary_index)
if(tracked_intercept_summary_index EQUAL -1)
    message(FATAL_ERROR "tracked_intercept summary did not contain intercept_success")
endif()

file(READ "${runtime_root}/assets/sample-aar/unguided_intercept/session-summary.md" unguided_intercept_summary)
string(FIND "${unguided_intercept_summary}" "assessment_code: timeout_observed" unguided_intercept_summary_index)
if(unguided_intercept_summary_index EQUAL -1)
    message(FATAL_ERROR "unguided_intercept summary did not contain timeout_observed")
endif()

file(READ "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept-state.json" tracked_intercept_golden_state)
foreach(expected
        "\"schema_version\": \"icss-gui-viewer-golden-state-v1\""
        "\"assessment_code\": \"intercept_success\""
        "\"effective_track_state\": \"tracked\""
        "\"intercept_profile\": \"tracked_intercept\"")
    string(FIND "${tracked_intercept_golden_state}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "tracked_intercept golden-state did not contain expected fragment: ${expected}")
    endif()
endforeach()

file(READ "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept-state.json" unguided_intercept_golden_state)
foreach(expected
        "\"schema_version\": \"icss-gui-viewer-golden-state-v1\""
        "\"assessment_code\": \"timeout_observed\""
        "\"effective_track_state\": \"untracked\""
        "\"intercept_profile\": \"unguided_intercept\"")
    string(FIND "${unguided_intercept_golden_state}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        message(FATAL_ERROR "unguided_intercept golden-state did not contain expected fragment: ${expected}")
    endif()
endforeach()

file(SIZE "${runtime_root}/assets/screenshots/tactical-display-tracked_intercept.bmp" tracked_intercept_screenshot_size)
file(SIZE "${runtime_root}/assets/screenshots/tactical-display-unguided_intercept.bmp" unguided_intercept_screenshot_size)
if(tracked_intercept_screenshot_size LESS 1)
    message(FATAL_ERROR "tracked_intercept screenshot was empty")
endif()
if(unguided_intercept_screenshot_size LESS 1)
    message(FATAL_ERROR "unguided_intercept screenshot was empty")
endif()
