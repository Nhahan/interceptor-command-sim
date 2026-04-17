cmake_minimum_required(VERSION 3.20)

project(interceptor_command_sim
    VERSION 0.1.0
    DESCRIPTION "Server-authoritative real-time simulation/control baseline"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

function(target_common target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(icss_protocol INTERFACE)
target_include_directories(icss_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_library(icss_core
    common/src/config.cpp
    common/src/frame_codec.cpp
    common/src/ascii_tactical_view.cpp
    common/src/runtime.cpp
    common/src/serialization.cpp
    common/src/transport.cpp
    common/src/simulation.cpp)
target_link_libraries(icss_core PUBLIC icss_protocol)
target_include_directories(icss_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_compile_definitions(icss_core PUBLIC ICSS_REPO_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\")
target_common(icss_core)

add_executable(icss_server server/src/main.cpp)
target_link_libraries(icss_server PRIVATE icss_core)
target_common(icss_server)

add_executable(icss_command_console clients/command-console/src/main.cpp)
target_link_libraries(icss_command_console PRIVATE icss_core)
target_common(icss_command_console)

add_executable(icss_tactical_viewer clients/tactical-viewer/src/main.cpp)
target_link_libraries(icss_tactical_viewer PRIVATE icss_core)
target_common(icss_tactical_viewer)

add_executable(icss_protocol_smoke tests/protocol/src/protocol_smoke.cpp)
target_link_libraries(icss_protocol_smoke PRIVATE icss_protocol)
target_common(icss_protocol_smoke)

add_executable(icss_payload_codec_smoke tests/protocol/src/payload_codec_smoke.cpp)
target_link_libraries(icss_payload_codec_smoke PRIVATE icss_core)
target_common(icss_payload_codec_smoke)

add_executable(icss_frame_codec_smoke tests/protocol/src/frame_codec_smoke.cpp)
target_link_libraries(icss_frame_codec_smoke PRIVATE icss_core)
target_common(icss_frame_codec_smoke)

add_executable(icss_transport_backend_smoke tests/protocol/src/transport_backend_smoke.cpp)
target_link_libraries(icss_transport_backend_smoke PRIVATE icss_core)
target_common(icss_transport_backend_smoke)

add_executable(icss_socket_live_single_session_policy_smoke tests/protocol/src/socket_live_single_session_policy_smoke.cpp)
target_link_libraries(icss_socket_live_single_session_policy_smoke PRIVATE icss_core)
target_common(icss_socket_live_single_session_policy_smoke)

add_executable(icss_scenario_flow tests/scenario/src/scenario_flow.cpp)
target_link_libraries(icss_scenario_flow PRIVATE icss_core)
target_common(icss_scenario_flow)

add_executable(icss_validation_rejects_invalid_flow tests/scenario/src/validation_rejects_invalid_flow.cpp)
target_link_libraries(icss_validation_rejects_invalid_flow PRIVATE icss_core)
target_common(icss_validation_rejects_invalid_flow)

add_executable(icss_runtime_config_smoke tests/scenario/src/runtime_config_smoke.cpp)
target_link_libraries(icss_runtime_config_smoke PRIVATE icss_core)
target_common(icss_runtime_config_smoke)

add_executable(icss_runtime_artifact_paths_smoke tests/scenario/src/runtime_artifact_paths_smoke.cpp)
target_link_libraries(icss_runtime_artifact_paths_smoke PRIVATE icss_core)
target_common(icss_runtime_artifact_paths_smoke)

add_executable(icss_session_log_smoke tests/scenario/src/session_log_smoke.cpp)
target_link_libraries(icss_session_log_smoke PRIVATE icss_core)
target_common(icss_session_log_smoke)

add_executable(icss_replay_cursor_smoke tests/scenario/src/replay_cursor_smoke.cpp)
target_link_libraries(icss_replay_cursor_smoke PRIVATE icss_core)
target_common(icss_replay_cursor_smoke)

add_executable(icss_resilience_smoke tests/resilience/src/resilience_smoke.cpp)
target_link_libraries(icss_resilience_smoke PRIVATE icss_core)
target_common(icss_resilience_smoke)

add_executable(icss_timeout_smoke tests/resilience/src/timeout_smoke.cpp)
target_link_libraries(icss_timeout_smoke PRIVATE icss_core)
target_common(icss_timeout_smoke)

add_executable(icss_socket_live_viewer_timeout_smoke tests/resilience/src/socket_live_viewer_timeout_smoke.cpp)
target_link_libraries(icss_socket_live_viewer_timeout_smoke PRIVATE icss_core)
target_common(icss_socket_live_viewer_timeout_smoke)

add_executable(icss_socket_live_snapshot_batching_smoke tests/resilience/src/socket_live_snapshot_batching_smoke.cpp)
target_link_libraries(icss_socket_live_snapshot_batching_smoke PRIVATE icss_core)
target_common(icss_socket_live_snapshot_batching_smoke)

if(BUILD_TESTING)
    add_test(NAME protocol_smoke COMMAND icss_protocol_smoke)
    add_test(NAME payload_codec_smoke COMMAND icss_payload_codec_smoke)
    add_test(NAME frame_codec_smoke COMMAND icss_frame_codec_smoke)
    add_test(NAME transport_backend_smoke COMMAND icss_transport_backend_smoke)
    add_test(NAME socket_live_single_session_policy_smoke COMMAND icss_socket_live_single_session_policy_smoke)
    add_test(NAME scenario_flow COMMAND icss_scenario_flow)
    add_test(NAME validation_rejects_invalid_flow COMMAND icss_validation_rejects_invalid_flow)
    add_test(NAME runtime_config_smoke COMMAND icss_runtime_config_smoke)
    add_test(NAME runtime_artifact_paths_smoke COMMAND icss_runtime_artifact_paths_smoke)
    add_test(NAME session_log_smoke COMMAND icss_session_log_smoke)
    add_test(NAME replay_cursor_smoke COMMAND icss_replay_cursor_smoke)
    add_test(NAME resilience_smoke COMMAND icss_resilience_smoke)
    add_test(NAME timeout_smoke COMMAND icss_timeout_smoke)
    add_test(NAME socket_live_viewer_timeout_smoke COMMAND icss_socket_live_viewer_timeout_smoke)
    add_test(NAME socket_live_snapshot_batching_smoke COMMAND icss_socket_live_snapshot_batching_smoke)
endif()
