if(NOT DEFINED ICSS_SERVER)
    message(FATAL_ERROR "ICSS_SERVER is required")
endif()

if(NOT DEFINED REPO_ROOT)
    message(FATAL_ERROR "REPO_ROOT is required")
endif()

string(RANDOM LENGTH 8 ALPHABET "0123456789abcdef" runtime_suffix)
set(runtime_root "${CMAKE_CURRENT_BINARY_DIR}/expect-server-success-${runtime_suffix}")

macro(fail_with_cleanup message_text)
    file(REMOVE_RECURSE "${runtime_root}")
    message(FATAL_ERROR "${message_text}")
endmacro()

file(REMOVE_RECURSE "${runtime_root}")
file(MAKE_DIRECTORY "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/server.example.yaml" DESTINATION "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/scenario.example.yaml" DESTINATION "${runtime_root}/configs")
file(COPY "${REPO_ROOT}/configs/logging.example.yaml" DESTINATION "${runtime_root}/configs")

set(arg_list)
if(DEFINED ARGS AND NOT ARGS STREQUAL "")
    string(REPLACE "|" ";" arg_list "${ARGS}")
endif()
list(APPEND arg_list --repo-root "${runtime_root}")

set(expected_list)
if(DEFINED EXPECTED AND NOT EXPECTED STREQUAL "")
    string(REPLACE "|" ";" expected_list "${EXPECTED}")
endif()

execute_process(
    COMMAND "${ICSS_SERVER}" ${arg_list}
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
    fail_with_cleanup("expected server command to succeed")
endif()

foreach(expected IN LISTS expected_list)
    string(FIND "${combined_output}" "${expected}" expected_index)
    if(expected_index EQUAL -1)
        fail_with_cleanup("expected output fragment not found: ${expected}")
    endif()
endforeach()

file(REMOVE_RECURSE "${runtime_root}")
