#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
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

std::vector<std::string> recv_udp_messages(int fd, std::size_t max_messages) {
    std::vector<std::string> messages;
    char buffer[512];
    sockaddr_in from {};
    socklen_t len = sizeof(from);
    for (std::size_t i = 0; i < max_messages; ++i) {
        const auto received = ::recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &len);
        if (received <= 0) break;
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
    const fs::path temp_root = fs::temp_directory_path() / ("icss_socket_batch_test_" + unique_suffix);
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    auto config = default_runtime_config(temp_root);
    config.server.tcp_port = 0;
    config.server.udp_port = 0;
    config.server.udp_send_latest_only = true;
    config.server.udp_max_batch_snapshots = 1;

    auto live = make_transport(BackendKind::SocketLive, config);
    const auto info = live->info();
    assert(info.binds_network);

    SocketGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(udp_viewer.fd >= 0);
    set_timeout(udp_viewer.fd);

    sockaddr_in udp_bind_addr {};
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_port = htons(0);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &udp_bind_addr.sin_addr) == 1);
    assert(::bind(udp_viewer.fd, reinterpret_cast<sockaddr*>(&udp_bind_addr), sizeof(udp_bind_addr)) == 0);

    // Advance simulation before registering viewer so multiple snapshots are pending.
    live->advance_tick();
    live->advance_tick();
    live->advance_tick();

    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(info.udp_port);
    assert(::inet_pton(AF_INET, config.server.bind_host.c_str(), &udp_server_addr.sin_addr) == 1);

    const auto viewer_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_viewer"});
    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    for (int i = 0; i < 5; ++i) {
        live->poll_once();
    }
    live->archive_session();

    const auto messages = wait_for_udp_messages(*live, udp_viewer.fd, 4);
    std::size_t snapshot_count = 0;
    std::size_t telemetry_count = 0;
    std::uint64_t snapshot_sequence = 0;
    for (const auto& wire : messages) {
        if (wire.rfind("kind=world_snapshot", 0) == 0) {
            const auto payload = parse_snapshot(wire);
            snapshot_sequence = payload.header.snapshot_sequence;
            ++snapshot_count;
        } else if (wire.rfind("kind=telemetry", 0) == 0) {
            ++telemetry_count;
        }
    }
    assert(snapshot_count >= 1);
    assert(snapshot_count <= static_cast<std::size_t>(config.server.udp_max_batch_snapshots));
    assert(telemetry_count >= 1);
    assert(telemetry_count <= static_cast<std::size_t>(config.server.udp_max_batch_snapshots));
    assert(snapshot_sequence == live->latest_snapshot().header.snapshot_sequence);

    fs::remove_all(temp_root);
#endif
    return 0;
}
