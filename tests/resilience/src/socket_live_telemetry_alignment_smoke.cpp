#include <cassert>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"

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
    ~SocketGuard() { if (fd >= 0) ::close(fd); }
};

void set_timeout(int fd) {
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void send_binary_frame(int fd, std::string_view kind, std::string_view payload) {
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    assert(::send(fd, frame.data(), frame.size(), 0) >= 0);
}

icss::protocol::FramedMessage wait_for_binary_frame(icss::net::TransportBackend& backend, int fd, int attempts = 20) {
    std::string buffer;
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        std::vector<std::uint8_t> bytes(4096);
        const auto received = ::recv(fd, bytes.data(), bytes.size(), 0);
        if (received > 0) {
            buffer.append(reinterpret_cast<const char*>(bytes.data()), static_cast<std::size_t>(received));
            icss::protocol::FramedMessage frame;
            if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
                return frame;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {};
}

std::vector<std::string> recv_udp_messages(int fd, std::size_t max_messages) {
    std::vector<std::string> messages;
    char buffer[4096];
    sockaddr_in from {};
    for (std::size_t i = 0; i < max_messages; ++i) {
        socklen_t len = sizeof(from);
        const auto received = ::recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &len);
        if (received <= 0) {
            break;
        }
        messages.emplace_back(buffer, buffer + received);
    }
    return messages;
}

std::vector<std::string> wait_for_udp_messages(icss::net::TransportBackend& backend, int fd, std::size_t max_messages, int attempts = 12) {
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        const auto messages = recv_udp_messages(fd, max_messages);
        if (!messages.empty()) {
            return messages;
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

    const auto unique_suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path temp_root = fs::temp_directory_path() / ("icss_socket_telemetry_alignment_" + unique_suffix);
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    auto config = default_runtime_config(temp_root);
    config.server.tcp_port = 0;
    config.server.udp_port = 0;
    config.server.tcp_frame_format = "binary";
    config.server.udp_send_latest_only = false;
    config.server.udp_max_batch_snapshots = 2;

    auto live = make_transport(BackendKind::SocketLive, config);
    const auto info = live->info();

    SocketGuard tcp_client(::socket(AF_INET, SOCK_STREAM, 0));
    SocketGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(tcp_client.fd >= 0);
    assert(udp_viewer.fd >= 0);
    set_timeout(tcp_client.fd);
    set_timeout(udp_viewer.fd);

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

    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(info.udp_port);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &udp_server_addr.sin_addr) == 1);

    send_binary_frame(tcp_client.fd, "session_join", serialize(SessionJoinPayload{{1001U, 101U, 1U}, "fire_control_console"}));
    assert(parse_command_ack(wait_for_binary_frame(*live, tcp_client.fd).payload).accepted);
    send_binary_frame(tcp_client.fd, "scenario_start", serialize(ScenarioStartPayload{{1001U, 101U, 2U}, config.scenario.name}));
    assert(parse_command_ack(wait_for_binary_frame(*live, tcp_client.fd).payload).accepted);
    send_binary_frame(tcp_client.fd, "track_acquire", serialize(TrackAcquirePayload{{1001U, 101U, 3U}, "target-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(*live, tcp_client.fd).payload).accepted);
    live->advance_tick();
    send_binary_frame(tcp_client.fd, "interceptor_ready", serialize(InterceptorReadyPayload{{1001U, 101U, 4U}, "interceptor-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(*live, tcp_client.fd).payload).accepted);
    send_binary_frame(tcp_client.fd, "engage_order", serialize(EngageOrderPayload{{1001U, 101U, 5U}, "interceptor-alpha", "target-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(*live, tcp_client.fd).payload).accepted);
    live->advance_tick();
    live->advance_tick();

    const auto viewer_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_display"});
    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    const auto messages = wait_for_udp_messages(*live, udp_viewer.fd, 8);
    assert(messages.size() >= 2);

    std::optional<SnapshotPayload> pending_snapshot;
    bool checked_pair = false;
    for (const auto& wire : messages) {
        if (wire.rfind("kind=world_snapshot", 0) == 0) {
            pending_snapshot = parse_snapshot(wire);
        } else if (wire.rfind("kind=telemetry", 0) == 0 && pending_snapshot.has_value()) {
            const auto telemetry = parse_telemetry(wire);
            assert(telemetry.event_tick <= pending_snapshot->header.tick);
            checked_pair = true;
            pending_snapshot.reset();
        }
    }
    assert(checked_pair);

    fs::remove_all(temp_root);
#endif
    return 0;
}
