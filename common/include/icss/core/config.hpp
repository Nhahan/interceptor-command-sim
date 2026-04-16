#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace icss::core {

struct ServerConfig {
    int tick_rate_hz {20};
    int tcp_port {4000};
    int udp_port {4001};
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

RuntimeConfig load_runtime_config(const std::filesystem::path& repo_root);

}  // namespace icss::core
