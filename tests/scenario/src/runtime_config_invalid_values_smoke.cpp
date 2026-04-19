#include <cassert>
#include <filesystem>
#include <stdexcept>

#include "icss/core/runtime.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    auto config = default_runtime_config(fs::path{ICSS_REPO_ROOT});

    config.server.tick_rate_hz = 0;
    bool rejected_tick_rate = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_tick_rate = std::string(error.what()).find("server.tick_rate_hz") != std::string::npos;
    }
    assert(rejected_tick_rate);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.heartbeat_interval_ms = 1000;
    config.server.heartbeat_timeout_ms = 500;
    bool rejected_heartbeat = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_heartbeat = std::string(error.what()).find("heartbeat_timeout_ms") != std::string::npos;
    }
    assert(rejected_heartbeat);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.udp_max_batch_snapshots = 0;
    bool rejected_batch = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_batch = std::string(error.what()).find("udp_max_batch_snapshots") != std::string::npos;
    }
    assert(rejected_batch);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.tcp_port = -1;
    bool rejected_port = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_port = std::string(error.what()).find("server.tcp_port") != std::string::npos;
    }
    assert(rejected_port);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.tcp_frame_format = "invalid";
    bool rejected_frame = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_frame = std::string(error.what()).find("server.tcp_frame_format") != std::string::npos;
    }
    assert(rejected_frame);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.bind_host = "999.0.0.1";
    bool rejected_bind_host = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_bind_host = std::string(error.what()).find("server.bind_host") != std::string::npos;
    }
    assert(rejected_bind_host);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.server.max_clients = 0;
    bool rejected_clients = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_clients = std::string(error.what()).find("server.max_clients") != std::string::npos;
    }
    assert(rejected_clients);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.scenario.telemetry_interval_ms = 0;
    bool rejected_telemetry = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_telemetry = std::string(error.what()).find("scenario.telemetry_interval_ms") != std::string::npos;
    }
    assert(rejected_telemetry);

    config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    config.scenario.launch_angle_deg = 91;
    bool rejected_launch_angle = false;
    try {
        validate_runtime_config(config);
    } catch (const std::runtime_error& error) {
        rejected_launch_angle = std::string(error.what()).find("scenario.launch_angle_deg") != std::string::npos;
    }
    assert(rejected_launch_angle);

    return 0;
}
