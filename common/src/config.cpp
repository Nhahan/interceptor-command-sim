#include "icss/core/config.hpp"

#include <fstream>
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
    return text == "true";
}

int parse_int(const std::string& text) {
    return std::stoi(text);
}

ServerConfig load_server_config(const std::filesystem::path& file) {
    ServerConfig config;
    for (const auto& raw_line : read_lines(file)) {
        const auto line = trim(raw_line);
        if (line.empty()) continue;
        if (starts_with(line, "tick_rate_hz:")) config.tick_rate_hz = parse_int(value_after_colon(line));
        else if (starts_with(line, "tcp:")) config.tcp_port = parse_int(value_after_colon(line));
        else if (starts_with(line, "udp:")) config.udp_port = parse_int(value_after_colon(line));
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
    return config;
}

}  // namespace icss::core
