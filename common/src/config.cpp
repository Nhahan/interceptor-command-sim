#include "icss/core/config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace icss::core {
namespace {

std::string trim(std::string value) {
    const auto start = value.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t");
    value = value.substr(start, end - start + 1);
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::vector<std::string> read_lines(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) {
        throw std::runtime_error("failed to open config: " + file.string());
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        lines.push_back(line);
    }
    return lines;
}

bool starts_with(std::string_view line, std::string_view key) {
    return line.substr(0, key.size()) == key;
}

std::string value_after_colon(const std::string& line) {
    const auto pos = line.find(':');
    if (pos == std::string::npos) {
        return {};
    }
    return trim(line.substr(pos + 1));
}

bool parse_bool(const std::string& text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    throw std::runtime_error("boolean value must be true or false");
}

int parse_int(const std::string& text) {
    return std::stoi(text);
}

void require_positive(std::string_view field, int value) {
    if (value <= 0) {
        throw std::runtime_error(std::string(field) + " must be greater than zero");
    }
}

void require_non_negative_port(std::string_view field, int value) {
    if (value < 0 || value > 65535) {
        throw std::runtime_error(std::string(field) + " must be between 0 and 65535");
    }
}

void validate_ipv4_host(const std::string& host) {
    std::stringstream ss(host);
    std::string octet;
    int count = 0;
    while (std::getline(ss, octet, '.')) {
        if (octet.empty() || octet.size() > 3) {
            throw std::runtime_error("server.bind_host must be a valid IPv4 address");
        }
        for (char ch : octet) {
            if (ch < '0' || ch > '9') {
                throw std::runtime_error("server.bind_host must be a valid IPv4 address");
            }
        }
        const int value = std::stoi(octet);
        if (value < 0 || value > 255) {
            throw std::runtime_error("server.bind_host must be a valid IPv4 address");
        }
        ++count;
    }
    if (count != 4) {
        throw std::runtime_error("server.bind_host must be a valid IPv4 address");
    }
}

ServerConfig load_server_config(const std::filesystem::path& file) {
    ServerConfig config;
    for (const auto& raw_line : read_lines(file)) {
        const auto line = trim(raw_line);
        if (line.empty()) continue;
        if (starts_with(line, "bind_host:")) config.bind_host = value_after_colon(line);
        else if (starts_with(line, "tcp_frame_format:")) config.tcp_frame_format = value_after_colon(line);
        else if (starts_with(line, "tick_rate_hz:")) config.tick_rate_hz = parse_int(value_after_colon(line));
        else if (starts_with(line, "tcp:")) config.tcp_port = parse_int(value_after_colon(line));
        else if (starts_with(line, "udp:")) config.udp_port = parse_int(value_after_colon(line));
        else if (starts_with(line, "udp_max_batch_snapshots:")) config.udp_max_batch_snapshots = parse_int(value_after_colon(line));
        else if (starts_with(line, "udp_send_latest_only:")) config.udp_send_latest_only = parse_bool(value_after_colon(line));
        else if (starts_with(line, "interval_ms:")) config.heartbeat_interval_ms = parse_int(value_after_colon(line));
        else if (starts_with(line, "timeout_ms:")) config.heartbeat_timeout_ms = parse_int(value_after_colon(line));
        else if (starts_with(line, "max_clients:")) config.max_clients = parse_int(value_after_colon(line));
    }
    return config;
}

ScenarioConfig load_scenario_config(const std::filesystem::path& file) {
    ScenarioConfig config;
    for (const auto& raw_line : read_lines(file)) {
        const auto line = trim(raw_line);
        if (line.empty()) continue;
        if (starts_with(line, "name:")) config.name = value_after_colon(line);
        else if (starts_with(line, "description:")) config.description = value_after_colon(line);
        else if (starts_with(line, "targets:")) config.targets = parse_int(value_after_colon(line));
        else if (starts_with(line, "assets:")) config.assets = parse_int(value_after_colon(line));
        else if (starts_with(line, "enable_replay:")) config.enable_replay = parse_bool(value_after_colon(line));
        else if (starts_with(line, "telemetry_interval_ms:")) config.telemetry_interval_ms = parse_int(value_after_colon(line));
        else if (starts_with(line, "world_width:")) config.world_width = parse_int(value_after_colon(line));
        else if (starts_with(line, "world_height:")) config.world_height = parse_int(value_after_colon(line));
        else if (starts_with(line, "target_start_x:")) config.target_start_x = parse_int(value_after_colon(line));
        else if (starts_with(line, "target_start_y:")) config.target_start_y = parse_int(value_after_colon(line));
        else if (starts_with(line, "target_velocity_x:")) config.target_velocity_x = parse_int(value_after_colon(line));
        else if (starts_with(line, "target_velocity_y:")) config.target_velocity_y = parse_int(value_after_colon(line));
        else if (starts_with(line, "interceptor_start_x:")) config.interceptor_start_x = parse_int(value_after_colon(line));
        else if (starts_with(line, "interceptor_start_y:")) config.interceptor_start_y = parse_int(value_after_colon(line));
        else if (starts_with(line, "interceptor_speed_per_tick:")) config.interceptor_speed_per_tick = parse_int(value_after_colon(line));
        else if (starts_with(line, "intercept_radius:")) config.intercept_radius = parse_int(value_after_colon(line));
        else if (starts_with(line, "engagement_timeout_ticks:")) config.engagement_timeout_ticks = parse_int(value_after_colon(line));
        else if (starts_with(line, "seeker_fov_deg:")) config.seeker_fov_deg = parse_int(value_after_colon(line));
        else if (starts_with(line, "launch_angle_deg:")) config.launch_angle_deg = parse_int(value_after_colon(line));
    }
    return config;
}

LoggingConfig load_logging_config(const std::filesystem::path& file) {
    LoggingConfig config;
    config.outputs.clear();
    for (const auto& raw_line : read_lines(file)) {
        const auto line = trim(raw_line);
        if (line.empty()) continue;
        if (starts_with(line, "level:")) config.level = value_after_colon(line);
        else if (starts_with(line, "- ")) config.outputs.push_back(trim(line.substr(2)));
        else if (starts_with(line, "file_path:")) config.file_path = value_after_colon(line);
        else if (starts_with(line, "enabled:")) config.aar_enabled = parse_bool(value_after_colon(line));
        else if (starts_with(line, "output_dir:")) config.aar_output_dir = value_after_colon(line);
    }
    return config;
}

}  // namespace

RuntimeConfig load_runtime_config(const std::filesystem::path& repo_root) {
    RuntimeConfig config;
    config.repo_root = repo_root;
    config.server = load_server_config(repo_root / "configs/server.example.yaml");
    config.scenario = load_scenario_config(repo_root / "configs/scenario.example.yaml");
    config.logging = load_logging_config(repo_root / "configs/logging.example.yaml");
    validate_runtime_config(config);
    return config;
}

void validate_runtime_config(const RuntimeConfig& config) {
    validate_ipv4_host(config.server.bind_host);
    require_positive("server.tick_rate_hz", config.server.tick_rate_hz);
    require_non_negative_port("server.tcp_port", config.server.tcp_port);
    require_non_negative_port("server.udp_port", config.server.udp_port);
    require_positive("server.udp_max_batch_snapshots", config.server.udp_max_batch_snapshots);
    require_positive("server.heartbeat_interval_ms", config.server.heartbeat_interval_ms);
    require_positive("server.heartbeat_timeout_ms", config.server.heartbeat_timeout_ms);
    require_positive("server.max_clients", config.server.max_clients);
    if (config.server.heartbeat_timeout_ms < config.server.heartbeat_interval_ms) {
        throw std::runtime_error("server.heartbeat_timeout_ms must be greater than or equal to server.heartbeat_interval_ms");
    }
    if (config.server.tcp_frame_format != "json" && config.server.tcp_frame_format != "binary") {
        throw std::runtime_error("server.tcp_frame_format must be one of: json, binary");
    }

    require_positive("scenario.targets", config.scenario.targets);
    require_positive("scenario.assets", config.scenario.assets);
    require_positive("scenario.telemetry_interval_ms", config.scenario.telemetry_interval_ms);
    require_positive("scenario.world_width", config.scenario.world_width);
    require_positive("scenario.world_height", config.scenario.world_height);
    require_positive("scenario.interceptor_speed_per_tick", config.scenario.interceptor_speed_per_tick);
    require_positive("scenario.intercept_radius", config.scenario.intercept_radius);
    require_positive("scenario.engagement_timeout_ticks", config.scenario.engagement_timeout_ticks);
    require_positive("scenario.seeker_fov_deg", config.scenario.seeker_fov_deg);
    if (config.scenario.launch_angle_deg < 0 || config.scenario.launch_angle_deg > 90) {
        throw std::runtime_error("scenario.launch_angle_deg must be between 0 and 90");
    }
    if (config.scenario.target_start_x < 0 || config.scenario.target_start_x >= config.scenario.world_width) {
        throw std::runtime_error("scenario.target_start_x must fit inside world_width");
    }
    if (config.scenario.target_start_y < 0 || config.scenario.target_start_y >= config.scenario.world_height) {
        throw std::runtime_error("scenario.target_start_y must fit inside world_height");
    }
    if (config.scenario.interceptor_start_x != 0) {
        throw std::runtime_error("scenario.interceptor_start_x must remain fixed at 0");
    }
    if (config.scenario.interceptor_start_y != 0) {
        throw std::runtime_error("scenario.interceptor_start_y must remain fixed at 0");
    }

    if (config.logging.outputs.empty()) {
        throw std::runtime_error("logging.outputs must not be empty");
    }
}

}  // namespace icss::core
