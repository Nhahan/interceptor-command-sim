#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"
#include "tests/support/temp_repo.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <cerrno>
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

SocketGuard connect_tcp_client(const std::string& host, std::uint16_t port) {
    SocketGuard guard(::socket(AF_INET, SOCK_STREAM, 0));
    assert(guard.fd >= 0);
    set_timeout(guard.fd);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    assert(::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1);
    assert(::connect(guard.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    return guard;
}

SocketGuard bind_udp_client(const std::string& host) {
    SocketGuard guard(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(guard.fd >= 0);
    set_timeout(guard.fd);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    assert(::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1);
    assert(::bind(guard.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    return guard;
}

sockaddr_in make_udp_server_addr(const std::string& host, std::uint16_t port) {
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    assert(::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1);
    return addr;
}

void send_binary_frame(int fd, std::string_view kind, std::string_view payload) {
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    assert(::send(fd, frame.data(), frame.size(), 0) >= 0);
}

std::vector<std::uint8_t> recv_some(int fd) {
    std::vector<std::uint8_t> bytes(1024);
    const auto received = ::recv(fd, bytes.data(), bytes.size(), 0);
    if (received <= 0) {
        return {};
    }
    bytes.resize(static_cast<std::size_t>(received));
    return bytes;
}

icss::protocol::FramedMessage wait_for_binary_frame(icss::net::TransportBackend& backend, int fd, int attempts = 8) {
    std::string buffer;
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        const auto bytes = recv_some(fd);
        buffer.append(bytes.begin(), bytes.end());
        icss::protocol::FramedMessage frame;
        if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
            return frame;
        }
    }
    return {};
}

bool wait_for_disconnect(icss::net::TransportBackend& backend, int fd, int attempts = 8) {
    char byte = '\0';
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        const auto received = ::recv(fd, &byte, 1, 0);
        if (received == 0) {
            return true;
        }
        if (received < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }
    }
    return false;
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

bool saw_udp_kind(const std::vector<std::string>& messages, std::string_view prefix) {
    for (const auto& message : messages) {
        if (message.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}
#endif

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    using namespace icss::protocol;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_single_session_policy_test_");

    auto config = default_runtime_config(temp_root);
    config.server.tcp_port = 0;
    config.server.udp_port = 0;
    config.server.tcp_frame_format = "binary";

    auto live = make_transport(BackendKind::SocketLive, config);
    const auto info = live->info();
    assert(info.binds_network);

    auto command_client = connect_tcp_client(config.server.bind_host, info.tcp_port);
    auto viewer_primary = bind_udp_client(config.server.bind_host);
    auto viewer_secondary = bind_udp_client(config.server.bind_host);
    const auto udp_server_addr = make_udp_server_addr(config.server.bind_host, info.udp_port);

    const auto viewer_primary_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_viewer"});
    assert(::sendto(viewer_primary.fd, viewer_primary_join.data(), viewer_primary_join.size(), 0,
                    reinterpret_cast<const sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);

    send_binary_frame(command_client.fd,
                      "session_join",
                      serialize(SessionJoinPayload{{1001U, 101U, 1U}, "command_console"}));
    const auto join_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(join_frame.kind == "command_ack");
    assert(parse_command_ack(join_frame.payload).accepted);
    const auto reconnect_events_before_duplicate = std::count_if(
        live->events().begin(),
        live->events().end(),
        [](const icss::core::EventRecord& event) {
            return event.header.event_type == icss::protocol::EventType::ClientReconnected;
        });
    send_binary_frame(command_client.fd,
                      "session_join",
                      serialize(SessionJoinPayload{{1001U, 101U, 99U}, "command_console"}));
    const auto duplicate_join_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(duplicate_join_frame.kind == "command_ack");
    assert(parse_command_ack(duplicate_join_frame.payload).accepted);
    const auto reconnect_events_after_duplicate = std::count_if(
        live->events().begin(),
        live->events().end(),
        [](const icss::core::EventRecord& event) {
            return event.header.event_type == icss::protocol::EventType::ClientReconnected;
        });
    assert(reconnect_events_after_duplicate == reconnect_events_before_duplicate);

    auto duplicate_command_client = connect_tcp_client(config.server.bind_host, info.tcp_port);
    assert(wait_for_disconnect(*live, duplicate_command_client.fd));

    const auto viewer_secondary_join = serialize(SessionJoinPayload{{1001U, 202U, 1U}, "tactical_viewer"});
    assert(::sendto(viewer_secondary.fd, viewer_secondary_join.data(), viewer_secondary_join.size(), 0,
                    reinterpret_cast<const sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    live->poll_once();

    send_binary_frame(command_client.fd,
                      "scenario_start",
                      serialize(ScenarioStartPayload{{1001U, 101U, 2U}, config.scenario.name}));
    const auto start_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(start_frame.kind == "command_ack");
    assert(parse_command_ack(start_frame.payload).accepted);

    send_binary_frame(command_client.fd,
                      "track_request",
                      serialize(TrackRequestPayload{{1001U, 101U, 3U}, "target-alpha"}));
    const auto track_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(track_frame.kind == "command_ack");
    assert(parse_command_ack(track_frame.payload).accepted);
    live->advance_tick();

    const auto primary_messages = recv_udp_messages(viewer_primary.fd, 20);
    const auto secondary_messages = recv_udp_messages(viewer_secondary.fd, 20);
    assert(saw_udp_kind(primary_messages, "kind=world_snapshot"));
    assert(saw_udp_kind(primary_messages, "kind=telemetry"));
    assert(secondary_messages.empty());

    auto viewer_reregistered = bind_udp_client(config.server.bind_host);
    const auto viewer_reregister_join = serialize(SessionJoinPayload{{1001U, 201U, 2U}, "tactical_viewer"});
    assert(::sendto(viewer_reregistered.fd, viewer_reregister_join.data(), viewer_reregister_join.size(), 0,
                    reinterpret_cast<const sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    live->poll_once();
    (void)recv_udp_messages(viewer_primary.fd, 20);
    (void)recv_udp_messages(viewer_reregistered.fd, 20);

    send_binary_frame(command_client.fd,
                      "asset_activate",
                      serialize(AssetActivatePayload{{1001U, 101U, 4U}, "asset-interceptor"}));
    const auto asset_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(asset_frame.kind == "command_ack");
    assert(parse_command_ack(asset_frame.payload).accepted);
    {
        const auto resync_snapshot = live->latest_snapshot();
        const auto resync_frame = icss::view::render_tactical_frame(
            resync_snapshot,
            live->events(),
            icss::view::make_replay_cursor(live->events().size(), live->events().empty() ? 0 : live->events().size() - 1));
        assert(resync_snapshot.viewer_connection == ConnectionState::Reconnected);
        assert(resync_frame.find("connection=reconnected") != std::string::npos);
        assert(resync_frame.find("freshness=resync") != std::string::npos);
    }

    send_binary_frame(command_client.fd,
                      "command_issue",
                      serialize(CommandIssuePayload{{1001U, 101U, 5U}, "asset-interceptor", "target-alpha"}));
    const auto command_frame = wait_for_binary_frame(*live, command_client.fd);
    assert(command_frame.kind == "command_ack");
    assert(parse_command_ack(command_frame.payload).accepted);
    {
        const auto steady_snapshot = live->latest_snapshot();
        const auto steady_frame = icss::view::render_tactical_frame(
            steady_snapshot,
            live->events(),
            icss::view::make_replay_cursor(live->events().size(), live->events().empty() ? 0 : live->events().size() - 1));
        assert(steady_snapshot.viewer_connection == ConnectionState::Connected);
        assert(steady_frame.find("connection=connected") != std::string::npos);
        assert(steady_frame.find("freshness=fresh") != std::string::npos);
    }

    live->advance_tick();
    live->advance_tick();

    const auto old_viewer_messages = recv_udp_messages(viewer_primary.fd, 20);
    const auto reregistered_messages = recv_udp_messages(viewer_reregistered.fd, 20);
    assert(old_viewer_messages.empty());
    assert(saw_udp_kind(reregistered_messages, "kind=world_snapshot"));
    assert(saw_udp_kind(reregistered_messages, "kind=telemetry"));

    fs::remove_all(temp_root);
#endif
    return 0;
}
