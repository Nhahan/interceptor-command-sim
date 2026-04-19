#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace icss::core {

struct ServerConfig {
    std::string bind_host {"127.0.0.1"};
    std::string tcp_frame_format {"json"};
    int tick_rate_hz {20};
    int tcp_port {4000};
    int udp_port {4001};
    int udp_max_batch_snapshots {2};
    bool udp_send_latest_only {false};
    int heartbeat_interval_ms {1000};
    int heartbeat_timeout_ms {3000};
    int max_clients {8};
};

struct ScenarioConfig {
    std::string name {"basic_intercept_training"};
    std::string description {"representative single-scenario training flow"};
    int targets {1};
    int assets {1};
    bool enable_replay {true};
    int telemetry_interval_ms {200};
    int world_width {2304};
    int world_height {1536};
    int target_start_x {480};
    int target_start_y {1200};
    int target_velocity_x {5};
    int target_velocity_y {-3};
    int interceptor_start_x {0};
    int interceptor_start_y {0};
    int interceptor_speed_per_tick {32};
    int intercept_radius {24};
    int engagement_timeout_ticks {60};
    int seeker_fov_deg {45};
    int launch_angle_deg {45};
};

struct LoggingConfig {
    std::string level {"info"};
    std::vector<std::string> outputs {"stdout", "file"};
    std::filesystem::path file_path {"logs/session.log"};
    bool aar_enabled {true};
    std::filesystem::path aar_output_dir {"assets/sample-aar"};
};

struct RuntimeConfig {
    ServerConfig server;
    ScenarioConfig scenario;
    LoggingConfig logging;
    std::filesystem::path repo_root;
};

void validate_runtime_config(const RuntimeConfig& config);
RuntimeConfig load_runtime_config(const std::filesystem::path& repo_root);

}  // namespace icss::core
