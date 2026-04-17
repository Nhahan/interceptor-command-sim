#pragma once

#include <cassert>
#include <chrono>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "icss/protocol/frame_codec.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace icss::testsupport::process {

#if !defined(_WIN32)

struct FdGuard {
    int fd {-1};
    explicit FdGuard(int value = -1) : fd(value) {}
    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;
    FdGuard(FdGuard&& other) noexcept : fd(other.fd) { other.fd = -1; }
    FdGuard& operator=(FdGuard&& other) noexcept {
        if (this != &other) {
            if (fd >= 0) {
                ::close(fd);
            }
            fd = other.fd;
            other.fd = -1;
        }
        return *this;
    }
    ~FdGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

struct ChildProcess {
    pid_t pid {-1};
    int output_fd {-1};
    std::string buffered_output;

    ChildProcess() = default;
    ChildProcess(pid_t pid_value, int fd_value, std::string buffered = {})
        : pid(pid_value), output_fd(fd_value), buffered_output(std::move(buffered)) {}
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept
        : pid(other.pid), output_fd(other.output_fd), buffered_output(std::move(other.buffered_output)) {
        other.pid = -1;
        other.output_fd = -1;
    }
    ChildProcess& operator=(ChildProcess&& other) noexcept {
        if (this != &other) {
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
            pid = other.pid;
            output_fd = other.output_fd;
            buffered_output = std::move(other.buffered_output);
            other.pid = -1;
            other.output_fd = -1;
        }
        return *this;
    }

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

inline void set_timeout(int fd) {
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

inline std::optional<std::string> read_line_with_deadline(ChildProcess& child, std::chrono::steady_clock::time_point deadline) {
    auto extract_line = [&]() -> std::optional<std::string> {
        const auto newline = child.buffered_output.find('\n');
        if (newline == std::string::npos) {
            return std::nullopt;
        }
        std::string line = child.buffered_output.substr(0, newline);
        child.buffered_output.erase(0, newline + 1);
        return line;
    };

    if (auto line = extract_line(); line.has_value()) {
        return line;
    }

    char ch = '\0';
    while (std::chrono::steady_clock::now() < deadline) {
        const auto received = ::read(child.output_fd, &ch, 1);
        if (received > 0) {
            child.buffered_output.push_back(ch);
            if (auto line = extract_line(); line.has_value()) {
                return line;
            }
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

inline std::vector<std::string> read_remaining_lines_with_deadline(ChildProcess& child, std::chrono::steady_clock::time_point deadline) {
    std::vector<std::string> lines;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto line = read_line_with_deadline(child, deadline);
        if (!line.has_value()) {
            break;
        }
        lines.push_back(*line);
    }
    return lines;
}

inline int parse_named_u16(std::string_view line, std::string_view key) {
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

inline std::pair<bool, int> wait_for_exit(pid_t pid, std::chrono::steady_clock::time_point deadline) {
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

inline FdGuard connect_tcp_client(std::uint16_t tcp_port, std::chrono::seconds timeout = std::chrono::seconds(10)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        FdGuard tcp_client(::socket(AF_INET, SOCK_STREAM, 0));
        assert(tcp_client.fd >= 0);
        set_timeout(tcp_client.fd);

        sockaddr_in tcp_addr {};
        tcp_addr.sin_family = AF_INET;
        tcp_addr.sin_port = htons(tcp_port);
        assert(::inet_pton(AF_INET, "127.0.0.1", &tcp_addr.sin_addr) == 1);
        if (::connect(tcp_client.fd, reinterpret_cast<sockaddr*>(&tcp_addr), sizeof(tcp_addr)) == 0) {
            return tcp_client;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    assert(false && "failed to connect tcp client before deadline");
    return FdGuard {};
}

inline FdGuard bind_udp_client() {
    FdGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(udp_viewer.fd >= 0);
    set_timeout(udp_viewer.fd);

    sockaddr_in udp_bind_addr {};
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_port = htons(0);
    assert(::inet_pton(AF_INET, "127.0.0.1", &udp_bind_addr.sin_addr) == 1);
    assert(::bind(udp_viewer.fd, reinterpret_cast<sockaddr*>(&udp_bind_addr), sizeof(udp_bind_addr)) == 0);
    return udp_viewer;
}

inline sockaddr_in make_udp_server_addr(std::uint16_t udp_port) {
    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(udp_port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &udp_server_addr.sin_addr) == 1);
    return udp_server_addr;
}

inline void send_binary_frame(int fd, std::string_view kind, std::string_view payload) {
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    assert(::send(fd, frame.data(), frame.size(), 0) >= 0);
}

inline std::vector<std::uint8_t> recv_some(int fd) {
    std::vector<std::uint8_t> bytes(1024);
    const auto received = ::recv(fd, bytes.data(), bytes.size(), 0);
    if (received <= 0) {
        return {};
    }
    bytes.resize(static_cast<std::size_t>(received));
    return bytes;
}

inline icss::protocol::FramedMessage wait_for_binary_frame(int fd, int attempts = 20) {
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

inline void send_json_frame(int fd, std::string_view kind, std::string_view payload) {
    const auto frame = icss::protocol::encode_json_frame(kind, payload) + "\n";
    assert(::send(fd, frame.data(), frame.size(), 0) >= 0);
}

inline std::optional<std::string> recv_json_line(int fd) {
    std::string line;
    char ch = '\0';
    while (true) {
        const auto received = ::recv(fd, &ch, 1, 0);
        if (received <= 0) {
            return std::nullopt;
        }
        if (ch == '\n') {
            return line;
        }
        line.push_back(ch);
    }
}

inline icss::protocol::FramedMessage wait_for_json_frame(int fd, int attempts = 20) {
    for (int i = 0; i < attempts; ++i) {
        const auto line = recv_json_line(fd);
        if (line.has_value()) {
            return icss::protocol::decode_json_frame(*line);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return {};
}

inline std::vector<std::string> recv_udp_messages(int fd, std::size_t max_messages, int attempts = 20) {
    std::vector<std::string> messages;
    char buffer[512];
    sockaddr_in from {};
    socklen_t len = sizeof(from);
    for (int attempt = 0; attempt < attempts && messages.empty(); ++attempt) {
        for (std::size_t i = 0; i < max_messages; ++i) {
            const auto received = ::recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &len);
            if (received <= 0) {
                break;
            }
            messages.emplace_back(buffer, buffer + received);
        }
        if (messages.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    return messages;
}

struct ServerLaunchOptions {
    std::filesystem::path repo_root;
    std::string bind_host {"127.0.0.1"};
    std::string tcp_frame_format {"binary"};
    bool run_forever {false};
    bool udp_send_latest_only {false};
    std::uint64_t tick_limit {40};
    std::uint64_t tick_sleep_ms {20};
    std::uint64_t tick_rate_hz {20};
    std::uint64_t telemetry_interval_ms {200};
    std::uint64_t heartbeat_interval_ms {1000};
    std::uint64_t heartbeat_timeout_ms {10000};
    std::uint64_t udp_max_batch_snapshots {2};
    std::uint64_t max_clients {8};
    std::uint16_t tcp_port {0};
    std::uint16_t udp_port {0};
};

inline ChildProcess spawn_server_process(const ServerLaunchOptions& options) {
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
        const std::string tick_limit = std::to_string(options.tick_limit);
        const std::string tick_sleep_ms = std::to_string(options.tick_sleep_ms);
        const std::string tick_rate_hz = std::to_string(options.tick_rate_hz);
        const std::string telemetry_interval_ms = std::to_string(options.telemetry_interval_ms);
        const std::string heartbeat_interval_ms = std::to_string(options.heartbeat_interval_ms);
        const std::string heartbeat_timeout_ms = std::to_string(options.heartbeat_timeout_ms);
        const std::string udp_max_batch_snapshots = std::to_string(options.udp_max_batch_snapshots);
        const std::string max_clients = std::to_string(options.max_clients);
        const std::string tcp_port = std::to_string(options.tcp_port);
        const std::string udp_port = std::to_string(options.udp_port);
        std::vector<std::string> argv_storage {
            server_path,
            "--backend", "socket_live",
            "--repo-root", options.repo_root.string(),
            "--bind-host", options.bind_host,
            "--tcp-port", tcp_port,
            "--udp-port", udp_port,
            "--tick-sleep-ms", tick_sleep_ms,
            "--tick-rate-hz", tick_rate_hz,
            "--telemetry-interval-ms", telemetry_interval_ms,
            "--heartbeat-interval-ms", heartbeat_interval_ms,
            "--heartbeat-timeout-ms", heartbeat_timeout_ms,
            "--udp-max-batch-snapshots", udp_max_batch_snapshots,
            "--udp-send-latest-only", options.udp_send_latest_only ? "true" : "false",
            "--max-clients", max_clients,
            "--tcp-frame-format", options.tcp_frame_format,
        };
        if (options.run_forever) {
            argv_storage.emplace_back("--run-forever");
        } else {
            argv_storage.emplace_back("--tick-limit");
            argv_storage.emplace_back(tick_limit);
        }
        std::vector<char*> argv;
        argv.reserve(argv_storage.size() + 1);
        for (auto& value : argv_storage) {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        execv(server_path.c_str(), argv.data());
        std::perror("exec icss_server failed");
        std::_Exit(127);
    }

    ::close(pipe_fds[1]);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFL, 0) | O_NONBLOCK);
    return ChildProcess {pid, pipe_fds[0], {}};
}

inline std::uint16_t reserve_port(int socket_type) {
    const int fd = ::socket(AF_INET, socket_type, 0);
    assert(fd >= 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    socklen_t len = sizeof(addr);
    assert(::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) == 0);
    const auto port = ntohs(addr.sin_port);
    ::close(fd);
    return port;
}

inline std::uint16_t reserve_tcp_port() {
    return reserve_port(SOCK_STREAM);
}

inline std::uint16_t reserve_udp_port() {
    return reserve_port(SOCK_DGRAM);
}

struct StartupInfo {
    std::uint16_t tcp_port {0};
    std::uint16_t udp_port {0};
};

inline StartupInfo wait_for_startup(ChildProcess& child, std::chrono::seconds timeout = std::chrono::seconds(10)) {
    StartupInfo info {};
    bool startup_ready = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline && !startup_ready) {
        const auto line = read_line_with_deadline(child, deadline);
        if (!line.has_value()) {
            continue;
        }
        if (line->find("backend=socket_live") != std::string::npos) {
            const auto parsed_tcp = parse_named_u16(*line, "tcp_port");
            const auto parsed_udp = parse_named_u16(*line, "udp_port");
            assert(parsed_tcp > 0);
            assert(parsed_udp > 0);
            info.tcp_port = static_cast<std::uint16_t>(parsed_tcp);
            info.udp_port = static_cast<std::uint16_t>(parsed_udp);
        }
        if (*line == "startup_ready=true") {
            startup_ready = true;
        }
    }
    if (!startup_ready) {
        std::fprintf(stderr, "startup timeout; buffered output:\n%s\n", child.buffered_output.c_str());
    }
    assert(startup_ready);
    assert(info.tcp_port > 0);
    assert(info.udp_port > 0);
    return info;
}

inline void wait_for_startup_ready(ChildProcess& child, std::chrono::seconds timeout = std::chrono::seconds(10)) {
    bool startup_ready = false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline && !startup_ready) {
        const auto line = read_line_with_deadline(child, deadline);
        if (!line.has_value()) {
            continue;
        }
        if (*line == "startup_ready=true") {
            startup_ready = true;
        }
    }
    if (!startup_ready) {
        std::fprintf(stderr, "startup timeout; buffered output:\n%s\n", child.buffered_output.c_str());
    }
    assert(startup_ready);
}

#endif

}  // namespace icss::testsupport::process
