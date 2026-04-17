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
    int world_width {24};
    int world_height {16};
    int target_start_x {3};
    int target_start_y {13};
    int target_velocity_x {1};
    int target_velocity_y {-1};
    int interceptor_start_x {10};
    int interceptor_start_y {2};
    int interceptor_speed_per_tick {4};
    int intercept_radius {1};
    int engagement_timeout_ticks {8};
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
