cmake_minimum_required(VERSION 3.20)

project(interceptor_command_sim
    VERSION 0.1.0
    DESCRIPTION "Server-authoritative real-time simulation/control baseline"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(ICSS_ENABLE_GUI_VIEWER "Build the SDL-based GUI tactical viewer" ON)

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
    common/src/artifact_readers.cpp
    common/src/config.cpp
    common/src/frame_codec.cpp
    common/src/ascii_tactical_view.cpp
    common/src/runtime.cpp
    common/src/serialization.cpp
    common/src/transport.cpp
    common/src/transport_common.cpp
    common/src/transport_inprocess.cpp
    common/src/transport_socket_live.cpp
    common/src/simulation.cpp
    common/src/simulation_support.cpp
    common/src/simulation_state.cpp
    common/src/simulation_clients.cpp
    common/src/simulation_commands.cpp
    common/src/simulation_summary.cpp
    common/src/simulation_snapshot.cpp)
target_link_libraries(icss_core PUBLIC icss_protocol)
target_include_directories(icss_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common/include)
target_compile_definitions(icss_core PUBLIC ICSS_REPO_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\")
target_common(icss_core)

add_executable(icss_server server/src/main.cpp)
target_link_libraries(icss_server PRIVATE icss_core)
target_common(icss_server)

add_executable(icss_fire_control_console clients/command-console/src/main.cpp)
target_link_libraries(icss_fire_control_console PRIVATE icss_core)
target_common(icss_fire_control_console)

add_executable(icss_tactical_display clients/tactical-viewer/src/main.cpp)
target_link_libraries(icss_tactical_display PRIVATE icss_core)
target_common(icss_tactical_display)

if(ICSS_ENABLE_GUI_VIEWER)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ICSS_SDL2 QUIET sdl2)
        pkg_check_modules(ICSS_SDL2_TTF QUIET SDL2_ttf)
    endif()
    if(ICSS_SDL2_FOUND AND ICSS_SDL2_TTF_FOUND)
        add_executable(icss_tactical_display_gui
            clients/tactical-viewer-gui/src/main.cpp
            clients/tactical-viewer-gui/src/app_support.cpp
            clients/tactical-viewer-gui/src/app_args.cpp
            clients/tactical-viewer-gui/src/app_labels.cpp
            clients/tactical-viewer-gui/src/app_setup.cpp
            clients/tactical-viewer-gui/src/app_visual_state.cpp
            clients/tactical-viewer-gui/src/app_snapshot.cpp
            clients/tactical-viewer-gui/src/app_net.cpp
            clients/tactical-viewer-gui/src/app_control.cpp
            clients/tactical-viewer-gui/src/app_render.cpp
            clients/tactical-viewer-gui/src/app_layout.cpp
            clients/tactical-viewer-gui/src/app_render_context.cpp
            clients/tactical-viewer-gui/src/app_render_map.cpp
            clients/tactical-viewer-gui/src/app_render_status_panels.cpp
            clients/tactical-viewer-gui/src/app_render_controls.cpp
            clients/tactical-viewer-gui/src/app_render_panels.cpp
            clients/tactical-viewer-gui/src/app_render_dump.cpp)
        target_link_directories(icss_tactical_display_gui PRIVATE ${ICSS_SDL2_LIBRARY_DIRS} ${ICSS_SDL2_TTF_LIBRARY_DIRS})
        target_include_directories(icss_tactical_display_gui PRIVATE
            clients/tactical-viewer-gui/src
            ${ICSS_SDL2_INCLUDE_DIRS}
            ${ICSS_SDL2_TTF_INCLUDE_DIRS})
        set(ICSS_GUI_SDL_LIBS ${ICSS_SDL2_LIBRARIES} ${ICSS_SDL2_TTF_LIBRARIES})
        list(REMOVE_DUPLICATES ICSS_GUI_SDL_LIBS)
        target_link_libraries(icss_tactical_display_gui PRIVATE icss_core ${ICSS_GUI_SDL_LIBS})
        target_compile_options(icss_tactical_display_gui PRIVATE ${ICSS_SDL2_CFLAGS_OTHER} ${ICSS_SDL2_TTF_CFLAGS_OTHER})
        target_common(icss_tactical_display_gui)
    else()
        message(STATUS "Skipping icss_tactical_display_gui: SDL2/SDL2_ttf not available")
    endif()
endif()

add_executable(icss_artifact_summary tools/src/artifact_summary.cpp)
target_link_libraries(icss_artifact_summary PRIVATE icss_core)
target_common(icss_artifact_summary)

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

add_executable(icss_server_process_live_smoke tests/protocol/src/server_process_live_smoke.cpp)
target_link_libraries(icss_server_process_live_smoke PRIVATE icss_core)
target_common(icss_server_process_live_smoke)

add_executable(icss_server_process_live_json_smoke tests/protocol/src/server_process_live_json_smoke.cpp)
target_link_libraries(icss_server_process_live_json_smoke PRIVATE icss_core)
target_common(icss_server_process_live_json_smoke)

add_executable(icss_server_process_live_override_precedence_smoke tests/protocol/src/server_process_live_override_precedence_smoke.cpp)
target_link_libraries(icss_server_process_live_override_precedence_smoke PRIVATE icss_core)
target_common(icss_server_process_live_override_precedence_smoke)

add_executable(icss_server_process_live_run_forever_smoke tests/protocol/src/server_process_live_run_forever_smoke.cpp)
target_link_libraries(icss_server_process_live_run_forever_smoke PRIVATE icss_core)
target_common(icss_server_process_live_run_forever_smoke)

add_executable(icss_fire_control_console_socket_live_smoke tests/protocol/src/command_console_socket_live_smoke.cpp)
target_link_libraries(icss_fire_control_console_socket_live_smoke PRIVATE icss_core)
add_dependencies(icss_fire_control_console_socket_live_smoke icss_server icss_fire_control_console)
target_common(icss_fire_control_console_socket_live_smoke)

if(TARGET icss_tactical_display_gui)
    add_executable(icss_tactical_display_gui_live_smoke tests/protocol/src/tactical_viewer_gui_live_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_live_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_live_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_live_smoke)
    add_executable(icss_tactical_display_gui_command_feedback_smoke tests/protocol/src/tactical_viewer_gui_command_feedback_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_command_feedback_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_command_feedback_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_command_feedback_smoke)
    add_executable(icss_tactical_display_gui_aar_smoke tests/protocol/src/tactical_viewer_gui_aar_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_aar_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_aar_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_aar_smoke)
    add_executable(icss_tactical_display_gui_parameter_timeout_smoke tests/protocol/src/tactical_viewer_gui_parameter_timeout_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_parameter_timeout_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_parameter_timeout_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_parameter_timeout_smoke)
    add_executable(icss_tactical_display_gui_setup_next_start_smoke tests/protocol/src/tactical_viewer_gui_setup_next_start_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_setup_next_start_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_setup_next_start_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_setup_next_start_smoke)
    add_executable(icss_tactical_display_gui_auto_rejoin_smoke tests/protocol/src/tactical_viewer_gui_auto_rejoin_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_auto_rejoin_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_auto_rejoin_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_auto_rejoin_smoke)
    add_executable(icss_tactical_display_gui_repo_root_config_smoke tests/protocol/src/tactical_viewer_gui_repo_root_config_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_repo_root_config_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_repo_root_config_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_repo_root_config_smoke)
    add_executable(icss_tactical_display_gui_snapshot_ordering_smoke
        tests/protocol/src/tactical_viewer_gui_snapshot_ordering_smoke.cpp
        clients/tactical-viewer-gui/src/app_snapshot.cpp
        clients/tactical-viewer-gui/src/app_support.cpp
        clients/tactical-viewer-gui/src/app_labels.cpp)
    target_link_directories(icss_tactical_display_gui_snapshot_ordering_smoke PRIVATE ${ICSS_SDL2_LIBRARY_DIRS} ${ICSS_SDL2_TTF_LIBRARY_DIRS})
    target_include_directories(icss_tactical_display_gui_snapshot_ordering_smoke PRIVATE
        clients/tactical-viewer-gui/src
        ${ICSS_SDL2_INCLUDE_DIRS}
        ${ICSS_SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(icss_tactical_display_gui_snapshot_ordering_smoke PRIVATE icss_core ${ICSS_GUI_SDL_LIBS})
    target_compile_options(icss_tactical_display_gui_snapshot_ordering_smoke PRIVATE ${ICSS_SDL2_CFLAGS_OTHER} ${ICSS_SDL2_TTF_CFLAGS_OTHER})
    target_common(icss_tactical_display_gui_snapshot_ordering_smoke)
    add_executable(icss_tactical_display_gui_malformed_udp_smoke
        tests/protocol/src/tactical_viewer_gui_malformed_udp_smoke.cpp
        clients/tactical-viewer-gui/src/app_net.cpp
        clients/tactical-viewer-gui/src/app_snapshot.cpp
        clients/tactical-viewer-gui/src/app_support.cpp
        clients/tactical-viewer-gui/src/app_labels.cpp
        clients/tactical-viewer-gui/src/app_visual_state.cpp)
    target_link_directories(icss_tactical_display_gui_malformed_udp_smoke PRIVATE ${ICSS_SDL2_LIBRARY_DIRS} ${ICSS_SDL2_TTF_LIBRARY_DIRS})
    target_include_directories(icss_tactical_display_gui_malformed_udp_smoke PRIVATE
        clients/tactical-viewer-gui/src
        ${ICSS_SDL2_INCLUDE_DIRS}
        ${ICSS_SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(icss_tactical_display_gui_malformed_udp_smoke PRIVATE icss_core ${ICSS_GUI_SDL_LIBS})
    target_compile_options(icss_tactical_display_gui_malformed_udp_smoke PRIVATE ${ICSS_SDL2_CFLAGS_OTHER} ${ICSS_SDL2_TTF_CFLAGS_OTHER})
    target_common(icss_tactical_display_gui_malformed_udp_smoke)
    add_executable(icss_tactical_display_gui_relative_dump_paths_smoke tests/protocol/src/tactical_viewer_gui_relative_dump_paths_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_relative_dump_paths_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_relative_dump_paths_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_relative_dump_paths_smoke)
    add_executable(icss_tactical_display_gui_duplicate_start_reject_smoke tests/protocol/src/tactical_viewer_gui_duplicate_start_reject_smoke.cpp)
    target_link_libraries(icss_tactical_display_gui_duplicate_start_reject_smoke PRIVATE icss_core)
    add_dependencies(icss_tactical_display_gui_duplicate_start_reject_smoke icss_server icss_tactical_display_gui)
    target_common(icss_tactical_display_gui_duplicate_start_reject_smoke)
endif()

add_executable(icss_scenario_flow tests/scenario/src/scenario_flow.cpp)
target_link_libraries(icss_scenario_flow PRIVATE icss_core)
target_common(icss_scenario_flow)

add_executable(icss_scenario_branching_smoke tests/scenario/src/scenario_branching_smoke.cpp)
target_link_libraries(icss_scenario_branching_smoke PRIVATE icss_core)
target_common(icss_scenario_branching_smoke)

add_executable(icss_validation_rejects_invalid_flow tests/scenario/src/validation_rejects_invalid_flow.cpp)
target_link_libraries(icss_validation_rejects_invalid_flow PRIVATE icss_core)
target_common(icss_validation_rejects_invalid_flow)

add_executable(icss_runtime_config_smoke tests/scenario/src/runtime_config_smoke.cpp)
target_link_libraries(icss_runtime_config_smoke PRIVATE icss_core)
target_common(icss_runtime_config_smoke)

add_executable(icss_runtime_config_invalid_values_smoke tests/scenario/src/runtime_config_invalid_values_smoke.cpp)
target_link_libraries(icss_runtime_config_invalid_values_smoke PRIVATE icss_core)
target_common(icss_runtime_config_invalid_values_smoke)

add_executable(icss_server_cli_override_precedence_smoke tests/scenario/src/server_cli_override_precedence_smoke.cpp)
target_link_libraries(icss_server_cli_override_precedence_smoke PRIVATE icss_core)
target_common(icss_server_cli_override_precedence_smoke)

add_executable(icss_artifact_reader_smoke tests/scenario/src/artifact_reader_smoke.cpp)
target_link_libraries(icss_artifact_reader_smoke PRIVATE icss_core)
target_common(icss_artifact_reader_smoke)

add_executable(icss_artifact_summary_cli_smoke tests/scenario/src/artifact_summary_cli_smoke.cpp)
target_link_libraries(icss_artifact_summary_cli_smoke PRIVATE icss_core)
target_common(icss_artifact_summary_cli_smoke)

add_executable(icss_runtime_artifact_paths_smoke tests/scenario/src/runtime_artifact_paths_smoke.cpp)
target_link_libraries(icss_runtime_artifact_paths_smoke PRIVATE icss_core)
target_common(icss_runtime_artifact_paths_smoke)

add_executable(icss_runtime_artifact_determinism_smoke tests/scenario/src/runtime_artifact_determinism_smoke.cpp)
target_link_libraries(icss_runtime_artifact_determinism_smoke PRIVATE icss_core)
target_common(icss_runtime_artifact_determinism_smoke)

add_executable(icss_session_log_smoke tests/scenario/src/session_log_smoke.cpp)
target_link_libraries(icss_session_log_smoke PRIVATE icss_core)
target_common(icss_session_log_smoke)

add_executable(icss_example_output_relative_path_smoke tests/scenario/src/example_output_relative_path_smoke.cpp)
target_link_libraries(icss_example_output_relative_path_smoke PRIVATE icss_core)
target_common(icss_example_output_relative_path_smoke)

add_executable(icss_aar_artifacts_relative_dir_smoke tests/scenario/src/aar_artifacts_relative_dir_smoke.cpp)
target_link_libraries(icss_aar_artifacts_relative_dir_smoke PRIVATE icss_core)
target_common(icss_aar_artifacts_relative_dir_smoke)

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

add_executable(icss_socket_live_telemetry_alignment_smoke tests/resilience/src/socket_live_telemetry_alignment_smoke.cpp)
target_link_libraries(icss_socket_live_telemetry_alignment_smoke PRIVATE icss_core)
target_common(icss_socket_live_telemetry_alignment_smoke)

add_executable(icss_socket_live_snapshot_batching_smoke tests/resilience/src/socket_live_snapshot_batching_smoke.cpp)
target_link_libraries(icss_socket_live_snapshot_batching_smoke PRIVATE icss_core)
target_common(icss_socket_live_snapshot_batching_smoke)

if(BUILD_TESTING)
    add_test(NAME protocol_smoke COMMAND icss_protocol_smoke)
    add_test(NAME payload_codec_smoke COMMAND icss_payload_codec_smoke)
    add_test(NAME frame_codec_smoke COMMAND icss_frame_codec_smoke)
    add_test(NAME transport_backend_smoke COMMAND icss_transport_backend_smoke)
    add_test(NAME socket_live_single_session_policy_smoke COMMAND icss_socket_live_single_session_policy_smoke)
    add_test(NAME server_process_live_smoke COMMAND icss_server_process_live_smoke)
    add_test(NAME server_process_live_json_smoke COMMAND icss_server_process_live_json_smoke)
    add_test(NAME server_process_live_override_precedence_smoke COMMAND icss_server_process_live_override_precedence_smoke)
    add_test(NAME server_process_live_run_forever_smoke COMMAND icss_server_process_live_run_forever_smoke)
    add_test(NAME fire_control_console_socket_live_smoke COMMAND icss_fire_control_console_socket_live_smoke)
    if(TARGET icss_tactical_display_gui_live_smoke)
        add_test(NAME tactical_display_gui_live_smoke COMMAND icss_tactical_display_gui_live_smoke)
        add_test(NAME tactical_display_gui_command_feedback_smoke COMMAND icss_tactical_display_gui_command_feedback_smoke)
        add_test(NAME tactical_display_gui_aar_smoke COMMAND icss_tactical_display_gui_aar_smoke)
        add_test(NAME tactical_display_gui_parameter_timeout_smoke COMMAND icss_tactical_display_gui_parameter_timeout_smoke)
        add_test(NAME tactical_display_gui_setup_next_start_smoke COMMAND icss_tactical_display_gui_setup_next_start_smoke)
        add_test(NAME tactical_display_gui_auto_rejoin_smoke COMMAND icss_tactical_display_gui_auto_rejoin_smoke)
        add_test(NAME tactical_display_gui_repo_root_config_smoke COMMAND icss_tactical_display_gui_repo_root_config_smoke)
        add_test(NAME tactical_display_gui_snapshot_ordering_smoke COMMAND icss_tactical_display_gui_snapshot_ordering_smoke)
        add_test(NAME tactical_display_gui_malformed_udp_smoke COMMAND icss_tactical_display_gui_malformed_udp_smoke)
        add_test(NAME tactical_display_gui_relative_dump_paths_smoke COMMAND icss_tactical_display_gui_relative_dump_paths_smoke)
        add_test(NAME tactical_display_gui_duplicate_start_reject_smoke COMMAND icss_tactical_display_gui_duplicate_start_reject_smoke)
    endif()
    add_test(NAME scenario_flow COMMAND icss_scenario_flow)
    add_test(NAME scenario_branching_smoke COMMAND icss_scenario_branching_smoke)
    add_test(NAME validation_rejects_invalid_flow COMMAND icss_validation_rejects_invalid_flow)
    add_test(NAME runtime_config_smoke COMMAND icss_runtime_config_smoke)
    add_test(NAME runtime_config_invalid_values_smoke COMMAND icss_runtime_config_invalid_values_smoke)
    add_test(NAME server_cli_override_precedence_smoke COMMAND icss_server_cli_override_precedence_smoke)
    add_test(NAME artifact_reader_smoke COMMAND icss_artifact_reader_smoke)
    add_test(NAME artifact_summary_cli_smoke COMMAND icss_artifact_summary_cli_smoke)
    add_test(NAME runtime_artifact_paths_smoke COMMAND icss_runtime_artifact_paths_smoke)
add_test(NAME runtime_artifact_determinism_smoke COMMAND icss_runtime_artifact_determinism_smoke)
add_test(NAME session_log_smoke COMMAND icss_session_log_smoke)
add_test(NAME example_output_relative_path_smoke COMMAND icss_example_output_relative_path_smoke)
add_test(NAME aar_artifacts_relative_dir_smoke COMMAND icss_aar_artifacts_relative_dir_smoke)
add_test(NAME replay_cursor_smoke COMMAND icss_replay_cursor_smoke)
    add_test(NAME resilience_smoke COMMAND icss_resilience_smoke)
    add_test(NAME timeout_smoke COMMAND icss_timeout_smoke)
    add_test(NAME socket_live_viewer_timeout_smoke COMMAND icss_socket_live_viewer_timeout_smoke)
    add_test(NAME socket_live_telemetry_alignment_smoke COMMAND icss_socket_live_telemetry_alignment_smoke)
    add_test(NAME socket_live_snapshot_batching_smoke COMMAND icss_socket_live_snapshot_batching_smoke)
    add_test(
        NAME server_inprocess_cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DARGS=--backend|in_process
            -DEXPECTED=backend=in_process|heartbeat_timeout_ms=|fire_control_console_connection=connected|display_connection=connected|last_event_type=session_ended|AAR\ artifacts\ written\ to
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_success.cmake)
    add_test(
        NAME server_socket_live_cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DARGS=--backend|socket_live|--tcp-port|0|--udp-port|0|--tick-limit|2|--tick-rate-hz|30|--telemetry-interval-ms|150|--heartbeat-interval-ms|600|--heartbeat-timeout-ms|1800|--udp-max-batch-snapshots|1|--udp-send-latest-only|true|--max-clients|4
            -DEXPECTED=backend=socket_live|binds_network=true|tick_rate_hz=30|telemetry_interval_ms=150|heartbeat_interval_ms=600|heartbeat_timeout_ms=1800|udp_send_latest_only=true|udp_max_batch_snapshots=1|max_clients=4|fire_control_console_connection=disconnected|display_connection=disconnected|last_event_type=resilience_triggered|latest_freshness=stale|latest_snapshot_sequence=2
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_success.cmake)
    add_test(
        NAME server_socket_live_idle_cli_smoke
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DARGS=--backend|socket_live|--tcp-port|0|--udp-port|0|--tick-limit|0
            -DEXPECTED=backend=socket_live|phase=standby|snapshots=0|fire_control_console_connection=disconnected|display_connection=disconnected|last_event_type=none|latest_freshness=stale|latest_snapshot_sequence=none
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_success.cmake)
    add_test(
        NAME server_invalid_backend_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|invalid
            -DEXPECTED=unsupported\ backend
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_sample_mode_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|in_process|--sample-mode|invalid
            -DEXPECTED=unsupported\ sample\ mode
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_frame_format_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|socket_live|--tick-limit|1|--tcp-frame-format|invalid
            -DEXPECTED=server.tcp_frame_format
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_port_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|socket_live|--tcp-port|-1
            -DEXPECTED=server.tcp_port
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_bind_host_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|socket_live|--bind-host|999.0.0.1
            -DEXPECTED=server.bind_host
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_heartbeat_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|socket_live|--heartbeat-interval-ms|1000|--heartbeat-timeout-ms|500
            -DEXPECTED=server.heartbeat_timeout_ms
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME server_invalid_batch_cli
        COMMAND ${CMAKE_COMMAND}
            -DICSS_SERVER=$<TARGET_FILE:icss_server>
            -DARGS=--backend|socket_live|--udp-max-batch-snapshots|0
            -DEXPECTED=server.udp_max_batch_snapshots
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_server_failure.cmake)
    add_test(
        NAME live_demo_script_smoke
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_live_demo.sh
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_live_demo_success.cmake)
    add_test(
        NAME sample_regen_script_smoke
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_live_demo.sh
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_sample_regen_success.cmake)
    add_test(
        NAME checked_in_sample_bundle_drift_smoke
        COMMAND ${CMAKE_COMMAND}
            -DDEMO_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_live_demo.sh
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/expect_checked_in_sample_bundle_matches_regen.cmake)
endif()
