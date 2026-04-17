#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "tests/support/process_smoke.hpp"
#include "tests/support/temp_repo.hpp"

namespace {

void write_text(const std::filesystem::path& file, const std::string& text) {
    std::ofstream out(file);
    out << text;
}

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    namespace process = icss::testsupport::process;

    const auto temp_root = icss::testsupport::make_temp_configured_repo("icss_server_process_precedence_");
    write_text(
        temp_root / "configs/server.example.yaml",
        "bind_host: 127.0.0.2\n"
        "tcp_frame_format: binary\n"
        "tick_rate_hz: 11\n"
        "udp_max_batch_snapshots: 3\n"
        "udp_send_latest_only: false\n"
        "ports:\n"
        "  tcp: 4100\n"
        "  udp: 4101\n"
        "heartbeat:\n"
        "  interval_ms: 700\n"
        "  timeout_ms: 1400\n"
        "session:\n"
        "  max_clients: 9\n");
    write_text(
        temp_root / "configs/scenario.example.yaml",
        "name: precedence_process_smoke\n"
        "description: precedence process smoke\n"
        "targets: 1\n"
        "assets: 1\n"
        "enable_replay: true\n"
        "telemetry_interval_ms: 250\n");

    auto child = process::spawn_server_process({
        .repo_root = temp_root,
        .bind_host = "127.0.0.1",
        .tcp_frame_format = "json",
        .tick_limit = 2,
        .tick_sleep_ms = 5,
        .tick_rate_hz = 30,
        .telemetry_interval_ms = 150,
        .heartbeat_interval_ms = 600,
        .heartbeat_timeout_ms = 1800,
        .udp_max_batch_snapshots = 1,
        .udp_send_latest_only = true,
        .max_clients = 4,
        .tcp_port = 0,
        .udp_port = 0,
    });

    bool startup_ready = false;
    bool saw_bind_host = false;
    bool saw_frame_format = false;
    bool saw_tick_rate = false;
    bool saw_telemetry_interval = false;
    bool saw_heartbeat_interval = false;
    bool saw_heartbeat_timeout = false;
    bool saw_batch = false;
    bool saw_send_latest = false;
    bool saw_max_clients = false;
    bool saw_no_file_bind_host = true;
    bool saw_no_file_frame = true;
    bool saw_no_file_tick = true;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline && !startup_ready) {
        const auto line = process::read_line_with_deadline(child, deadline);
        if (!line.has_value()) {
            continue;
        }
        saw_bind_host = saw_bind_host || line->find("bind_host=127.0.0.1") != std::string::npos;
        saw_frame_format = saw_frame_format || line->find("frame_format=json") != std::string::npos;
        saw_tick_rate = saw_tick_rate || line->find("tick_rate_hz=30") != std::string::npos;
        saw_telemetry_interval = saw_telemetry_interval || line->find("telemetry_interval_ms=150") != std::string::npos;
        saw_heartbeat_interval = saw_heartbeat_interval || line->find("heartbeat_interval_ms=600") != std::string::npos;
        saw_heartbeat_timeout = saw_heartbeat_timeout || line->find("heartbeat_timeout_ms=1800") != std::string::npos;
        saw_batch = saw_batch || line->find("udp_max_batch_snapshots=1") != std::string::npos;
        saw_send_latest = saw_send_latest || line->find("udp_send_latest_only=true") != std::string::npos;
        saw_max_clients = saw_max_clients || line->find("max_clients=4") != std::string::npos;

        saw_no_file_bind_host = saw_no_file_bind_host && line->find("bind_host=127.0.0.2") == std::string::npos;
        saw_no_file_frame = saw_no_file_frame && line->find("frame_format=binary") == std::string::npos;
        saw_no_file_tick = saw_no_file_tick && line->find("tick_rate_hz=11") == std::string::npos;

        if (*line == "startup_ready=true") {
            startup_ready = true;
        }
    }

    assert(startup_ready);
    assert(saw_bind_host);
    assert(saw_frame_format);
    assert(saw_tick_rate);
    assert(saw_telemetry_interval);
    assert(saw_heartbeat_interval);
    assert(saw_heartbeat_timeout);
    assert(saw_batch);
    assert(saw_send_latest);
    assert(saw_max_clients);
    assert(saw_no_file_bind_host);
    assert(saw_no_file_frame);
    assert(saw_no_file_tick);

    const auto [exited, _status] = process::wait_for_exit(child.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(exited);
    child.pid = -1;
    fs::remove_all(temp_root);
#endif
    return 0;
}
