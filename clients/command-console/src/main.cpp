#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/messages.hpp"
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

std::filesystem::path resolve_repo_root(std::filesystem::path path) {
    if (std::filesystem::exists(path / "configs/server.example.yaml")) {
        return path;
    }
    if (std::filesystem::exists(path.parent_path() / "configs/server.example.yaml")) {
        return path.parent_path();
    }
    return path;
}

struct ConsoleOptions {
    std::string backend {"in_process"};
    std::string host {"127.0.0.1"};
    std::uint16_t tcp_port {4000};
    std::string tcp_frame_format {"json"};
    std::uint32_t session_id {1001U};
    std::uint32_t sender_id {101U};
    std::string scenario_name {"basic_intercept_training"};
    std::filesystem::path repo_root {std::filesystem::current_path()};
};

ConsoleOptions parse_args(int argc, char** argv) {
    ConsoleOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto require_value = [&](std::string_view label) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(label));
            }
            return argv[++i];
        };
        if (arg == "--backend") {
            options.backend = require_value(arg);
            continue;
        }
        if (arg == "--host") {
            options.host = require_value(arg);
            continue;
        }
        if (arg == "--tcp-port") {
            options.tcp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--tcp-frame-format") {
            options.tcp_frame_format = require_value(arg);
            continue;
        }
        if (arg == "--session-id") {
            options.session_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--sender-id") {
            options.sender_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--scenario-name") {
            options.scenario_name = require_value(arg);
            continue;
        }
        if (arg == "--repo-root") {
            options.repo_root = require_value(arg);
            continue;
        }
        if (arg == "--help") {
            std::cout
                << "usage: icss_fire_control_console [--backend in_process|socket_live] [--host HOST] [--tcp-port PORT]\n"
                << "                           [--tcp-frame-format json|binary] [--session-id ID] [--sender-id ID]\n"
                << "                           [--scenario-name NAME] [--repo-root PATH]\n";
            std::exit(0);
        }
        throw std::runtime_error("unknown argument: " + std::string(arg));
    }
    return options;
}

#if !defined(_WIN32)
enum class FrameMode {
    Json,
    Binary,
};

FrameMode parse_frame_mode(std::string_view value) {
    if (value == "json") {
        return FrameMode::Json;
    }
    if (value == "binary") {
        return FrameMode::Binary;
    }
    throw std::runtime_error("unsupported tcp frame format: " + std::string(value));
}

class TcpSocket {
public:
    TcpSocket(const std::string& host, std::uint16_t port) {
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
            throw std::runtime_error("failed to connect to socket_live server");
        }
    }

    ~TcpSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    [[nodiscard]] int fd() const { return fd_; }

private:
    int fd_ {-1};
};

void send_frame(int fd, FrameMode mode, std::string_view kind, std::string_view payload) {
    if (mode == FrameMode::Json) {
        const auto line = icss::protocol::encode_json_frame(kind, payload) + "\n";
        if (::send(fd, line.data(), line.size(), 0) < 0) {
            throw std::runtime_error("failed to send json frame");
        }
        return;
    }
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    if (::send(fd, frame.data(), frame.size(), 0) < 0) {
        throw std::runtime_error("failed to send binary frame");
    }
}

icss::protocol::FramedMessage recv_frame(int fd, FrameMode mode) {
    if (mode == FrameMode::Json) {
        std::string line;
        char ch = '\0';
        while (true) {
            const auto received = ::recv(fd, &ch, 1, 0);
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
        const auto received = ::recv(fd, chunk, sizeof(chunk), 0);
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

void print_ack(std::string_view kind, const icss::protocol::CommandAckPayload& ack) {
    std::cout << kind << ": " << (ack.accepted ? "accepted" : "rejected")
              << " | reason=" << ack.reason << '\n';
}

int run_socket_live(const ConsoleOptions& options) {
    const auto frame_mode = parse_frame_mode(options.tcp_frame_format);
    TcpSocket socket(options.host, options.tcp_port);
    const auto runtime_config = icss::core::load_runtime_config(resolve_repo_root(options.repo_root));
    const auto& scenario = runtime_config.scenario;
    std::uint64_t sequence = 1;

    const auto send_and_expect_ack = [&](std::string_view kind, std::string payload) {
        send_frame(socket.fd(), frame_mode, kind, payload);
        const auto frame = recv_frame(socket.fd(), frame_mode);
        if (frame.kind != "command_ack") {
            throw std::runtime_error("expected command_ack after " + std::string(kind));
        }
        const auto ack = icss::protocol::parse_command_ack(frame.payload);
        print_ack(kind, ack);
        if (!ack.accepted) {
            throw std::runtime_error(std::string(kind) + " rejected: " + ack.reason);
        }
        return ack;
    };

    std::cout << "Command console live mode\n";
    std::cout << "backend=socket_live, host=" << options.host << ", tcp_port=" << options.tcp_port
              << ", frame_format=" << options.tcp_frame_format << '\n';

    send_and_expect_ack(
        "session_join",
        icss::protocol::serialize(icss::protocol::SessionJoinPayload{{options.session_id, options.sender_id, sequence++}, "fire_control_console"}));
    send_and_expect_ack(
        "scenario_start",
        icss::protocol::serialize(icss::protocol::ScenarioStartPayload{
            {options.session_id, options.sender_id, sequence++},
            options.scenario_name,
            scenario.world_width,
            scenario.world_height,
            scenario.target_start_x,
            scenario.target_start_y,
            scenario.target_velocity_x,
            scenario.target_velocity_y,
            scenario.interceptor_start_x,
            scenario.interceptor_start_y,
            scenario.interceptor_speed_per_tick,
            scenario.intercept_radius,
            scenario.engagement_timeout_ticks,
            scenario.seeker_fov_deg,
            scenario.launch_angle_deg,
        }));
    send_and_expect_ack(
        "track_acquire",
        icss::protocol::serialize(icss::protocol::TrackAcquirePayload{{options.session_id, options.sender_id, sequence++}, "target-alpha"}));
    send_and_expect_ack(
        "interceptor_ready",
        icss::protocol::serialize(icss::protocol::InterceptorReadyPayload{{options.session_id, options.sender_id, sequence++}, "interceptor-alpha"}));
    send_and_expect_ack(
        "engage_order",
        icss::protocol::serialize(icss::protocol::EngageOrderPayload{{options.session_id, options.sender_id, sequence++}, "interceptor-alpha", "target-alpha"}));

    icss::protocol::AarResponsePayload aar;
    bool aar_ready = false;
    const auto max_attempts = std::max(20, scenario.engagement_timeout_ticks * 4);
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        send_frame(socket.fd(),
                   frame_mode,
                   "aar_request",
                   icss::protocol::serialize(icss::protocol::AarRequestPayload{{options.session_id, options.sender_id, sequence++}, 999U, "absolute"}));
        const auto aar_frame = recv_frame(socket.fd(), frame_mode);
        if (aar_frame.kind != "aar_response") {
            throw std::runtime_error("expected aar_response after aar_request");
        }
        aar = icss::protocol::parse_aar_response(aar_frame.payload);
        if (aar.assessment_code != "pending") {
            aar_ready = true;
            break;
        }
    }
    if (!aar_ready) {
        throw std::runtime_error("aar did not reach a terminal assessment before timeout");
    }
    std::cout << "aar_response: cursor=" << aar.replay_cursor_index << "/" << aar.total_events
              << " | assessment_code=" << aar.assessment_code
              << " | event_type=" << aar.event_type << '\n';
    return 0;
}
#endif

int run_in_process() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    using namespace icss::protocol;

    auto transport = make_transport(BackendKind::InProcess, default_runtime_config(fs::path{ICSS_REPO_ROOT}));
    transport->connect_client(ClientRole::FireControlConsole, 101U);

    const std::vector<std::pair<std::string_view, CommandResult>> commands {
        {to_string(TcpMessageKind::ScenarioStart), transport->start_scenario()},
        {to_string(TcpMessageKind::TrackAcquire), transport->dispatch(TrackAcquirePayload{{1001U, 101U, 1U}, "target-alpha"})},
        {to_string(TcpMessageKind::InterceptorReady), transport->dispatch(InterceptorReadyPayload{{1001U, 101U, 2U}, "interceptor-alpha"})},
        {to_string(TcpMessageKind::EngageOrder), transport->dispatch(EngageOrderPayload{{1001U, 101U, 3U}, "interceptor-alpha", "target-alpha"})},
    };

    std::cout << "Command console baseline\n";
    std::cout << "backend=" << transport->backend_name() << '\n';
    for (const auto& [label, result] : commands) {
        std::cout << label << ": " << (result.accepted ? "accepted" : "rejected")
                  << " | reason=" << result.reason << '\n';
    }

    const EngageOrderPayload preview {
        {1001U, 101U, 4U},
        "interceptor-alpha",
        "target-alpha",
    };
    const AarRequestPayload aar_preview {
        {1001U, 101U, 5U},
        11U,
    };
    std::cout << "wire_preview.engage_order=" << serialize(preview) << '\n';
    std::cout << "wire_preview.aar_request=" << serialize(aar_preview) << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_args(argc, argv);
        if (options.backend == "in_process") {
            return run_in_process();
        }
#if !defined(_WIN32)
        if (options.backend == "socket_live") {
            return run_socket_live(options);
        }
#endif
        throw std::runtime_error("unsupported backend: " + options.backend);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
