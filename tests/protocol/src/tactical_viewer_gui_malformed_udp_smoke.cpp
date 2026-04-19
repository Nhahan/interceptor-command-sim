#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>

#include "clients/tactical-viewer-gui/src/app.hpp"
#include "icss/protocol/serialization.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

#if !defined(_WIN32)
std::uint16_t bound_port(int fd) {
    sockaddr_in addr {};
    socklen_t len = sizeof(addr);
    const auto rc = ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    assert(rc == 0);
    return ntohs(addr.sin_port);
}

void send_udp(std::uint16_t port, std::string_view wire) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    const auto ok = ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    assert(ok == 1);
    const auto sent = ::sendto(fd,
                               wire.data(),
                               wire.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&addr),
                               sizeof(addr));
    assert(sent == static_cast<ssize_t>(wire.size()));
    ::close(fd);
}

void send_udp_from(std::uint16_t bind_port, std::uint16_t dest_port, std::string_view wire) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    assert(fd >= 0);
    sockaddr_in bind_addr {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(bind_port);
    const auto ok = ::inet_pton(AF_INET, "127.0.0.1", &bind_addr.sin_addr);
    assert(ok == 1);
    const auto bound = ::bind(fd, reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr));
    assert(bound == 0);

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    const auto sent = ::sendto(fd,
                               wire.data(),
                               wire.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&addr),
                               sizeof(addr));
    assert(sent == static_cast<ssize_t>(wire.size()));
    ::close(fd);
}
#endif

}  // namespace

int main() {
#if !defined(_WIN32)
    using namespace icss::viewer_gui;

    UdpSocket socket("127.0.0.1");
    ViewerState state;
    const auto port = bound_port(socket.get());
    ViewerOptions options;
    options.host = "127.0.0.1";
    options.udp_port = 47001;

    send_udp(port, "not-a-valid-udp-frame");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    receive_datagrams(socket, options, state, 1111U);
    assert(!state.received_snapshot);
    assert(!state.received_telemetry);
    assert(state.last_datagram_received_ms == 0U);

    const auto telemetry = icss::protocol::serialize(icss::protocol::TelemetryPayload{
        {1001U, 1U, 1U},
        {5U, 12U, 0.0F, 4444U},
        "connected",
        5U,
        "client_joined",
        "viewer registered",
    });
    send_udp_from(options.udp_port, port, telemetry);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    receive_datagrams(socket, options, state, 2222U);
    assert(state.received_telemetry);
    assert(state.last_datagram_received_ms == 2222U);
    const auto last_type = state.last_server_event_type;
    const auto last_summary = state.last_server_event_summary;
    const auto event_count = state.recent_server_events.size();

    send_udp(port, "kind=telemetry;broken_without_equals");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    receive_datagrams(socket, options, state, 3333U);
    assert(state.last_datagram_received_ms == 2222U);
    assert(state.last_server_event_type == last_type);
    assert(state.last_server_event_summary == last_summary);
    assert(state.recent_server_events.size() == event_count);
#endif
    return 0;
}
