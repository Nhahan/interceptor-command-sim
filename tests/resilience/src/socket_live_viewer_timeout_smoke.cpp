#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
#if !defined(_WIN32)
struct SocketGuard {
    int fd {-1};
    explicit SocketGuard(int value = -1) : fd(value) {}
    ~SocketGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

void set_timeout(int fd) {
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void send_line(int fd, const std::string& line) {
    const auto payload = line + "\n";
    ::send(fd, payload.data(), payload.size(), 0);
}

std::string recv_line(int fd) {
    std::string line;
    char ch = '\0';
    while (true) {
        const auto received = ::recv(fd, &ch, 1, 0);
        if (received <= 0) {
            break;
        }
        if (ch == '\n') {
            break;
        }
        line.push_back(ch);
    }
    return line;
}

std::string wait_for_line(icss::net::TransportBackend& backend, int fd, int attempts = 8) {
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        const auto line = recv_line(fd);
        if (!line.empty()) {
            return line;
        }
    }
    return {};
}
#endif
}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    using namespace icss::protocol;

    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path temp_root = fs::temp_directory_path() / ("icss_socket_timeout_test_" + unique_suffix);
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    auto config = default_runtime_config(temp_root);
    config.server.tcp_port = 0;
    config.server.udp_port = 0;
    config.server.heartbeat_timeout_ms = 400;
    config.scenario.telemetry_interval_ms = 200;

    auto live = make_transport(BackendKind::SocketLive, config);
    const auto info = live->info();
    assert(info.binds_network);

    SocketGuard tcp_client(::socket(AF_INET, SOCK_STREAM, 0));
    SocketGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    SocketGuard udp_spoof(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(tcp_client.fd >= 0);
    assert(udp_viewer.fd >= 0);
    assert(udp_spoof.fd >= 0);
    set_timeout(tcp_client.fd);
    set_timeout(udp_viewer.fd);
    set_timeout(udp_spoof.fd);

    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(info.tcp_port);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &tcp_addr.sin_addr) == 1);
    assert(::connect(tcp_client.fd, reinterpret_cast<sockaddr*>(&tcp_addr), sizeof(tcp_addr)) == 0);

    sockaddr_in udp_bind_addr {};
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_port = htons(0);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &udp_bind_addr.sin_addr) == 1);
    assert(::bind(udp_viewer.fd, reinterpret_cast<sockaddr*>(&udp_bind_addr), sizeof(udp_bind_addr)) == 0);
    assert(::bind(udp_spoof.fd, reinterpret_cast<sockaddr*>(&udp_bind_addr), sizeof(udp_bind_addr)) == 0);

    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(info.udp_port);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &udp_server_addr.sin_addr) == 1);

    const auto viewer_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_viewer"});
    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);

    send_line(tcp_client.fd, encode_json_frame("session_join", serialize(SessionJoinPayload{{1001U, 101U, 1U}, "command_console"})));
    const auto join_ack = wait_for_line(*live, tcp_client.fd);
    assert(!join_ack.empty());

    send_line(tcp_client.fd, encode_json_frame("scenario_start", serialize(ScenarioStartPayload{{1001U, 101U, 2U}, config.scenario.name})));
    const auto start_ack = wait_for_line(*live, tcp_client.fd);
    assert(!start_ack.empty());

    const auto heartbeat = serialize(ViewerHeartbeatPayload{{1001U, 201U, 2U}, 1U});
    assert(::sendto(udp_viewer.fd, heartbeat.data(), heartbeat.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    live->poll_once();

    const auto spoofed_heartbeat = serialize(ViewerHeartbeatPayload{{1001U, 201U, 3U}, 2U});
    assert(::sendto(udp_spoof.fd, spoofed_heartbeat.data(), spoofed_heartbeat.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    live->poll_once();

    std::this_thread::sleep_for(std::chrono::milliseconds(650));
    for (int i = 0; i < 4; ++i) {
        live->advance_tick();
    }
    const auto snapshot = live->latest_snapshot();
    assert(snapshot.viewer_connection == ConnectionState::TimedOut);
    const auto frame = icss::view::render_tactical_frame(
        snapshot,
        live->events(),
        icss::view::make_replay_cursor(live->events().size(), live->events().empty() ? 0 : live->events().size() - 1));
    assert(frame.find("connection=timed_out") != std::string::npos);
    assert(frame.find("freshness=stale") != std::string::npos);
    assert(frame.find("snapshot_sequence=") != std::string::npos);
    const auto summary = live->summary();
    assert(summary.resilience_case.find("timeout_visibility") != std::string::npos);

    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    auto recovered = live->latest_snapshot();
    const auto recovery_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < recovery_deadline) {
        live->poll_once();
        live->advance_tick();
        recovered = live->latest_snapshot();
        if (recovered.viewer_connection == ConnectionState::Reconnected
            || recovered.viewer_connection == ConnectionState::Connected) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(recovered.viewer_connection == ConnectionState::Reconnected
           || recovered.viewer_connection == ConnectionState::Connected);

    fs::remove_all(temp_root);
#endif
    return 0;
}
