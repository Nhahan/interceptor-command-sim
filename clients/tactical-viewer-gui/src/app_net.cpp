#include "app.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "icss/protocol/serialization.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace icss::viewer_gui {

#if !defined(_WIN32)
namespace {

void set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("failed to set nonblocking socket");
    }
}

sockaddr_in make_server_addr(const ViewerOptions& options) {
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options.udp_port);
    if (::inet_pton(AF_INET, options.host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid host for udp server address");
    }
    return addr;
}

void send_datagram(int fd, const sockaddr_in& target, std::string_view payload) {
    const auto sent = ::sendto(fd,
                               payload.data(),
                               payload.size(),
                               0,
                               reinterpret_cast<const sockaddr*>(&target),
                               sizeof(target));
    if (sent < 0) {
        throw std::runtime_error("failed to send udp datagram");
    }
}

}  // namespace

FrameMode parse_frame_mode(std::string_view value) {
    if (value == "json") {
        return FrameMode::Json;
    }
    if (value == "binary") {
        return FrameMode::Binary;
    }
    throw std::runtime_error("unsupported tcp frame format: " + std::string(value));
}

UdpSocket::UdpSocket(const std::string& host) {
    fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("failed to create udp socket");
    }
    set_nonblocking(fd_);

    sockaddr_in bind_addr {};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(0);
    if (::inet_pton(AF_INET, host.c_str(), &bind_addr.sin_addr) != 1) {
        throw std::runtime_error("invalid host for udp bind");
    }
    if (::bind(fd_, reinterpret_cast<const sockaddr*>(&bind_addr), sizeof(bind_addr)) != 0) {
        throw std::runtime_error("failed to bind udp viewer socket");
    }
}

UdpSocket::~UdpSocket() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

int UdpSocket::get() const { return fd_; }

TcpSocket::~TcpSocket() {
    reset();
}

void TcpSocket::connect_to(const std::string& host, std::uint16_t port) {
    reset();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        throw std::runtime_error("failed to create tcp socket");
    }
    timeval timeout {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid host address");
    }
    if (::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error("failed to connect control tcp socket");
    }
}

void TcpSocket::reset() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool TcpSocket::connected() const { return fd_ >= 0; }
int TcpSocket::fd() const { return fd_; }

bool same_endpoint(const sockaddr_in& lhs, const sockaddr_in& rhs) {
    return lhs.sin_family == rhs.sin_family
        && lhs.sin_port == rhs.sin_port
        && lhs.sin_addr.s_addr == rhs.sin_addr.s_addr;
}

void send_viewer_join(UdpSocket& socket, const ViewerOptions& options, std::uint64_t& sequence) {
    const icss::protocol::SessionJoinPayload join {{options.session_id, options.sender_id, sequence++}, "tactical_viewer"};
    send_datagram(socket.get(), make_server_addr(options), icss::protocol::serialize(join));
}

void send_viewer_heartbeat(UdpSocket& socket, const ViewerOptions& options, std::uint64_t& sequence, ViewerState& state) {
    const icss::protocol::ViewerHeartbeatPayload heartbeat {{options.session_id, options.sender_id, sequence++}, ++state.heartbeat_id};
    send_datagram(socket.get(), make_server_addr(options), icss::protocol::serialize(heartbeat));
    ++state.heartbeat_count;
}

void receive_datagrams(UdpSocket& socket, const ViewerOptions& options, ViewerState& state, std::uint64_t now_ms) {
    char buffer[4096];
    sockaddr_in from {};
    while (true) {
        socklen_t len = sizeof(from);
        const auto received = ::recvfrom(socket.get(),
                                         buffer,
                                         sizeof(buffer),
                                         0,
                                         reinterpret_cast<sockaddr*>(&from),
                                         &len);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (!same_endpoint(from, make_server_addr(options))) {
            continue;
        }
        const std::string wire(buffer, buffer + received);
        if (wire.rfind("kind=world_snapshot", 0) == 0) {
            try {
                apply_snapshot(state, icss::protocol::parse_snapshot(wire));
                state.last_datagram_received_ms = now_ms;
            } catch (const std::exception&) {
                continue;
            }
            continue;
        }
        if (wire.rfind("kind=telemetry", 0) == 0) {
            try {
                apply_telemetry(state, icss::protocol::parse_telemetry(wire));
                state.last_datagram_received_ms = now_ms;
            } catch (const std::exception&) {
                continue;
            }
        }
    }
}

void send_frame(TcpSocket& socket, FrameMode mode, std::string_view kind, std::string_view payload) {
    if (mode == FrameMode::Json) {
        const auto line = icss::protocol::encode_json_frame(kind, payload) + "\n";
        if (::send(socket.fd(), line.data(), line.size(), 0) < 0) {
            throw std::runtime_error("failed to send json frame");
        }
        return;
    }
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    if (::send(socket.fd(), frame.data(), frame.size(), 0) < 0) {
        throw std::runtime_error("failed to send binary frame");
    }
}

icss::protocol::FramedMessage recv_frame(TcpSocket& socket, FrameMode mode) {
    if (mode == FrameMode::Json) {
        std::string line;
        char ch = '\0';
        while (true) {
            const auto received = ::recv(socket.fd(), &ch, 1, 0);
            if (received <= 0) {
                throw std::runtime_error("failed to receive json frame");
            }
            if (ch == '\n') {
                return icss::protocol::decode_json_frame(line);
            }
            line.push_back(ch);
        }
    }
    std::string buffer;
    char chunk[512];
    while (true) {
        const auto received = ::recv(socket.fd(), chunk, sizeof(chunk), 0);
        if (received <= 0) {
            throw std::runtime_error("failed to receive binary frame");
        }
        buffer.append(chunk, static_cast<std::size_t>(received));
        icss::protocol::FramedMessage frame;
        if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
            return frame;
        }
    }
}

void ensure_control_connected(TcpSocket& socket,
                              ViewerState& state,
                              const ViewerOptions& options,
                              FrameMode mode) {
    if (state.control.connected && socket.connected()) {
        return;
    }
    socket.connect_to(options.host, options.tcp_port);
    send_frame(socket,
               mode,
               "session_join",
               icss::protocol::serialize(icss::protocol::SessionJoinPayload{{options.session_id, options.sender_id, state.control.sequence++},
                                                                            "command_console"}));
    const auto frame = recv_frame(socket, mode);
    if (frame.kind != "command_ack") {
        throw std::runtime_error("expected command_ack for session_join");
    }
    const auto ack = icss::protocol::parse_command_ack(frame.payload);
    state.control.connected = ack.accepted;
    state.control.last_ok = ack.accepted;
    state.control.last_label = "join";
    state.control.last_message = ack.reason;
    if (!ack.accepted) {
        throw std::runtime_error("command console attach rejected: " + ack.reason);
    }
}

#else
FrameMode parse_frame_mode(std::string_view) { throw std::runtime_error("gui networking unsupported on this platform"); }
UdpSocket::UdpSocket(const std::string&) { throw std::runtime_error("gui networking unsupported on this platform"); }
UdpSocket::~UdpSocket() = default;
int UdpSocket::get() const { return -1; }
TcpSocket::~TcpSocket() = default;
void TcpSocket::connect_to(const std::string&, std::uint16_t) { throw std::runtime_error("gui networking unsupported on this platform"); }
void TcpSocket::reset() {}
bool TcpSocket::connected() const { return false; }
int TcpSocket::fd() const { return -1; }
void send_viewer_join(UdpSocket&, const ViewerOptions&, std::uint64_t&) {}
void send_viewer_heartbeat(UdpSocket&, const ViewerOptions&, std::uint64_t&, ViewerState&) {}
void receive_datagrams(UdpSocket&, const ViewerOptions&, ViewerState&, std::uint64_t) {}
void ensure_control_connected(TcpSocket&, ViewerState&, const ViewerOptions&, FrameMode) {}
void send_frame(TcpSocket&, FrameMode, std::string_view, std::string_view) {}
icss::protocol::FramedMessage recv_frame(TcpSocket&, FrameMode) { throw std::runtime_error("gui networking unsupported on this platform"); }
#endif

}  // namespace icss::viewer_gui
