cmake_minimum_required(VERSION 3.20)

project(interceptor_command_sim
    VERSION 0.1.0
    DESCRIPTION "Server-authoritative real-time simulation/control portfolio scaffold"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)

function(target_common target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(icss_protocol INTERFACE)
target_include_directories(icss_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_library(icss_core
    common/src/ascii_tactical_view.cpp
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

add_executable(icss_scenario_flow tests/scenario/src/scenario_flow.cpp)
target_link_libraries(icss_scenario_flow PRIVATE icss_core)
target_common(icss_scenario_flow)

add_executable(icss_validation_rejects_invalid_flow tests/scenario/src/validation_rejects_invalid_flow.cpp)
target_link_libraries(icss_validation_rejects_invalid_flow PRIVATE icss_core)
target_common(icss_validation_rejects_invalid_flow)

add_executable(icss_resilience_smoke tests/resilience/src/resilience_smoke.cpp)
target_link_libraries(icss_resilience_smoke PRIVATE icss_core)
target_common(icss_resilience_smoke)

if(BUILD_TESTING)
    add_test(NAME protocol_smoke COMMAND icss_protocol_smoke)
    add_test(NAME scenario_flow COMMAND icss_scenario_flow)
    add_test(NAME validation_rejects_invalid_flow COMMAND icss_validation_rejects_invalid_flow)
    add_test(NAME resilience_smoke COMMAND icss_resilience_smoke)
endif()
