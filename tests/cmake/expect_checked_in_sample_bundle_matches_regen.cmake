if(NOT DEFINED DEMO_SCRIPT)
    message(FATAL_ERROR "DEMO_SCRIPT is required")
endif()

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif()

set(runtime_root "${CMAKE_CURRENT_BINARY_DIR}/checked-in-sample-drift-smoke")
file(REMOVE_RECURSE "${runtime_root}")
file(MAKE_DIRECTORY "${runtime_root}")

macro(fail_with_cleanup message_text)
    file(REMOVE_RECURSE "${runtime_root}")
    message(FATAL_ERROR "${message_text}")
endmacro()

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
    fail_with_cleanup("expected sample regen drift check to succeed")
endif()

foreach(relative_path
        "assets/sample-aar/session-summary.md"
        "assets/sample-aar/session-summary.json"
        "assets/sample-aar/replay-timeline.json"
        "assets/sample-aar/straight/session-summary.md"
        "assets/sample-aar/straight/session-summary.json"
        "assets/sample-aar/straight/replay-timeline.json"
        "examples/sample-output.md"
        "examples/sample-output-straight.md"
        "assets/screenshots/tactical-viewer-guidance-state.json"
        "assets/screenshots/tactical-viewer-straight-state.json")
    set(generated_path "${runtime_root}/${relative_path}")
    set(checked_in_path "${REPO_ROOT}/${relative_path}")

    if(NOT EXISTS "${generated_path}")
        fail_with_cleanup("generated sample file missing: ${generated_path}")
    endif()
    if(NOT EXISTS "${checked_in_path}")
        fail_with_cleanup("checked-in sample file missing: ${checked_in_path}")
    endif()

    file(SHA256 "${generated_path}" generated_hash)
    file(SHA256 "${checked_in_path}" checked_in_hash)
    if(NOT generated_hash STREQUAL checked_in_hash)
        fail_with_cleanup("checked-in sample drift detected: ${relative_path}")
    endif()
endforeach()

foreach(relative_path
        "assets/screenshots/tactical-viewer-guidance.bmp"
        "assets/screenshots/tactical-viewer-straight.bmp")
    set(generated_path "${runtime_root}/${relative_path}")
    set(checked_in_path "${REPO_ROOT}/${relative_path}")
    if(NOT EXISTS "${generated_path}")
        fail_with_cleanup("generated screenshot missing: ${generated_path}")
    endif()
    if(NOT EXISTS "${checked_in_path}")
        fail_with_cleanup("checked-in screenshot missing: ${checked_in_path}")
    endif()
    file(SIZE "${generated_path}" generated_size)
    file(SIZE "${checked_in_path}" checked_in_size)
    if(generated_size LESS 1 OR checked_in_size LESS 1)
        fail_with_cleanup("screenshot file was empty: ${relative_path}")
    endif()
endforeach()

file(REMOVE_RECURSE "${runtime_root}")
