#include <cassert>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <thread>

#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "tests/support/temp_repo.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#if !defined(_WIN32)
struct FdGuard {
    int fd {-1};
    explicit FdGuard(int value = -1) : fd(value) {}
    ~FdGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

struct ChildProcess {
    pid_t pid {-1};
    int output_fd {-1};

    ~ChildProcess() {
        if (output_fd >= 0) {
            ::close(output_fd);
        }
        if (pid > 0) {
            int status = 0;
            const auto waited = ::waitpid(pid, &status, WNOHANG);
            if (waited == 0) {
                ::kill(pid, SIGTERM);
                ::waitpid(pid, &status, 0);
            }
        }
    }
};

std::pair<bool, int> wait_for_exit(pid_t pid, std::chrono::steady_clock::time_point deadline) {
    int status = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto waited = ::waitpid(pid, &status, WNOHANG);
        if (waited == pid) {
            return {true, status};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return {false, status};
}

void set_timeout(int fd) {
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

std::optional<std::string> read_line_with_deadline(int fd, std::chrono::steady_clock::time_point deadline) {
    std::string line;
    char ch = '\0';
    while (std::chrono::steady_clock::now() < deadline) {
        const auto received = ::read(fd, &ch, 1);
        if (received > 0) {
            if (ch == '\n') {
                return line;
            }
            line.push_back(ch);
            continue;
        }
        if (received == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        break;
    }
    return std::nullopt;
}

std::vector<std::string> read_remaining_lines_with_deadline(int fd, std::chrono::steady_clock::time_point deadline) {
    std::vector<std::string> lines;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto line = read_line_with_deadline(fd, deadline);
        if (!line.has_value()) {
            break;
        }
        lines.push_back(*line);
    }
    return lines;
}

int parse_named_u16(std::string_view line, std::string_view key) {
    const auto token = std::string(key) + "=";
    const auto start = line.find(token);
    if (start == std::string_view::npos) {
        return -1;
    }
    auto pos = start + token.size();
    std::string value;
    while (pos < line.size() && std::isdigit(static_cast<unsigned char>(line[pos]))) {
        value.push_back(line[pos++]);
    }
    return value.empty() ? -1 : std::stoi(value);
}

ChildProcess spawn_server_process(const std::filesystem::path& repo_root) {
    int pipe_fds[2] {};
    assert(::pipe(pipe_fds) == 0);
    const auto pid = ::fork();
    assert(pid >= 0);
    if (pid == 0) {
        ::close(pipe_fds[0]);
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::dup2(pipe_fds[1], STDERR_FILENO);
        ::close(pipe_fds[1]);

        const std::string server_path = std::filesystem::path{ICSS_REPO_ROOT} / "build/icss_server";
        execl(server_path.c_str(),
              server_path.c_str(),
              "--backend", "socket_live",
              "--repo-root", repo_root.c_str(),
              "--tcp-port", "0",
              "--udp-port", "0",
              "--run-forever",
              "--tick-sleep-ms", "20",
              "--heartbeat-interval-ms", "1000",
              "--heartbeat-timeout-ms", "10000",
              "--tcp-frame-format", "binary",
              static_cast<char*>(nullptr));
        std::_Exit(127);
    }

    ::close(pipe_fds[1]);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFL, 0) | O_NONBLOCK);
    return {pid, pipe_fds[0]};
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

icss::protocol::FramedMessage wait_for_binary_frame(int fd, int attempts = 20) {
    std::string buffer;
    for (int i = 0; i < attempts; ++i) {
        const auto bytes = recv_some(fd);
        buffer.append(bytes.begin(), bytes.end());
        icss::protocol::FramedMessage frame;
        if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
            return frame;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {};
}
#endif

}  // namespace

int main() {
#if !defined(_WIN32)
    using namespace icss::protocol;

    const auto temp_root = icss::testsupport::make_temp_configured_repo("icss_server_process_live_forever_");
    auto child = spawn_server_process(temp_root);

    std::uint16_t tcp_port = 0;
    std::uint16_t udp_port = 0;
    bool startup_ready = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline && !startup_ready) {
        const auto line = read_line_with_deadline(child.output_fd, deadline);
        if (!line.has_value()) {
            continue;
        }
        if (line->find("backend=socket_live") != std::string::npos) {
            const auto parsed_tcp = parse_named_u16(*line, "tcp_port");
            const auto parsed_udp = parse_named_u16(*line, "udp_port");
            assert(parsed_tcp > 0);
            assert(parsed_udp > 0);
            tcp_port = static_cast<std::uint16_t>(parsed_tcp);
            udp_port = static_cast<std::uint16_t>(parsed_udp);
        }
        if (*line == "startup_ready=true") {
            startup_ready = true;
        }
    }

    assert(startup_ready);
    assert(tcp_port > 0);
    assert(udp_port > 0);

    FdGuard tcp_client(::socket(AF_INET, SOCK_STREAM, 0));
    FdGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(tcp_client.fd >= 0);
    assert(udp_viewer.fd >= 0);
    set_timeout(tcp_client.fd);
    set_timeout(udp_viewer.fd);

    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(tcp_port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &tcp_addr.sin_addr) == 1);
    assert(::connect(tcp_client.fd, reinterpret_cast<sockaddr*>(&tcp_addr), sizeof(tcp_addr)) == 0);

    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(udp_port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &udp_server_addr.sin_addr) == 1);

    const auto viewer_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_display"});
    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    const auto display_heartbeat = serialize(DisplayHeartbeatPayload{{1001U, 201U, 2U}, 1U});
    assert(::sendto(udp_viewer.fd, display_heartbeat.data(), display_heartbeat.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);

    send_binary_frame(tcp_client.fd, "session_join", serialize(SessionJoinPayload{{1001U, 101U, 1U}, "fire_control_console"}));
    assert(parse_command_ack(wait_for_binary_frame(tcp_client.fd).payload).accepted);

    send_binary_frame(tcp_client.fd, "scenario_start", serialize(ScenarioStartPayload{{1001U, 101U, 2U}, "basic_intercept_training"}));
    assert(parse_command_ack(wait_for_binary_frame(tcp_client.fd).payload).accepted);

    send_binary_frame(tcp_client.fd, "track_acquire", serialize(TrackAcquirePayload{{1001U, 101U, 3U}, "target-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(tcp_client.fd).payload).accepted);

    send_binary_frame(tcp_client.fd, "interceptor_ready", serialize(InterceptorReadyPayload{{1001U, 101U, 4U}, "interceptor-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(tcp_client.fd).payload).accepted);

    send_binary_frame(tcp_client.fd, "engage_order", serialize(EngageOrderPayload{{1001U, 101U, 5U}, "interceptor-alpha", "target-alpha"}));
    assert(parse_command_ack(wait_for_binary_frame(tcp_client.fd).payload).accepted);

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(::kill(child.pid, SIGTERM) == 0);
    const auto [exited, status] = wait_for_exit(child.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(exited);
    child.pid = -1;
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    const auto trailing_lines = read_remaining_lines_with_deadline(child.output_fd, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    bool saw_shutdown_requested = false;
    bool saw_shutdown_reason = false;
    bool saw_event_summary = false;
    bool saw_artifact_line = false;
    for (const auto& line : trailing_lines) {
        saw_shutdown_requested = saw_shutdown_requested || line.find("shutdown_requested=true") != std::string::npos;
        saw_shutdown_reason = saw_shutdown_reason || line.find("shutdown_reason=signal") != std::string::npos;
        saw_event_summary = saw_event_summary || line.find("last_event_type=session_ended") != std::string::npos;
        saw_artifact_line = saw_artifact_line || line.find("AAR artifacts written to") != std::string::npos;
    }
    assert(saw_shutdown_requested);
    assert(saw_shutdown_reason);
    assert(saw_event_summary);
    assert(saw_artifact_line);
    assert(std::filesystem::exists(temp_root / "assets/sample-aar/replay-timeline.json"));
    assert(std::filesystem::exists(temp_root / "assets/sample-aar/session-summary.md"));
    assert(std::filesystem::exists(temp_root / "examples/sample-output.md"));
    assert(std::filesystem::exists(temp_root / "logs/session.log"));
    std::filesystem::remove_all(temp_root);
#endif
    return 0;
}
