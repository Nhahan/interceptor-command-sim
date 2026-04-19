#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tests/support/process_smoke.hpp"
#include "tests/support/temp_repo.hpp"

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

#if !defined(_WIN32)
using icss::testsupport::process::ChildProcess;

ChildProcess spawn_command_console(const std::filesystem::path& repo_root,
                                   std::uint16_t tcp_port,
                                   std::string frame_format) {
    int pipe_fds[2] {};
    assert(::pipe(pipe_fds) == 0);
    const auto pid = ::fork();
    assert(pid >= 0);
    if (pid == 0) {
        ::close(pipe_fds[0]);
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::dup2(pipe_fds[1], STDERR_FILENO);
        ::close(pipe_fds[1]);

        const std::string exe = (std::filesystem::path{ICSS_REPO_ROOT} / "build/icss_command_console").string();
        const std::string port = std::to_string(tcp_port);
        std::vector<std::string> argv_storage {
            exe,
            "--backend", "socket_live",
            "--host", "127.0.0.1",
            "--tcp-port", port,
            "--tcp-frame-format", std::move(frame_format),
            "--repo-root", repo_root.string(),
        };
        std::vector<char*> argv;
        argv.reserve(argv_storage.size() + 1);
        for (auto& value : argv_storage) {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        ::execv(exe.c_str(), argv.data());
        std::perror("exec icss_command_console failed");
        std::_Exit(127);
    }

    ::close(pipe_fds[1]);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFL, 0) | O_NONBLOCK);
    return ChildProcess {pid, pipe_fds[0], {}};
}
#endif

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    namespace process = icss::testsupport::process;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_command_console_live_");
    {
        const fs::path scenario_file = temp_root / "configs/scenario.example.yaml";
        std::ofstream out(scenario_file);
        out << "scenario:\n";
        out << "  name: basic_intercept_training\n";
        out << "  description: command console timeout verification\n";
        out << "entities:\n";
        out << "  targets: 1\n";
        out << "  assets: 1\n";
        out << "rules:\n";
        out << "  enable_replay: true\n";
        out << "  telemetry_interval_ms: 200\n";
        out << "  world_width: 576\n";
        out << "  world_height: 384\n";
        out << "  target_start_x: 80\n";
        out << "  target_start_y: 300\n";
        out << "  target_velocity_x: 9\n";
        out << "  target_velocity_y: -6\n";
        out << "  interceptor_start_x: 0\n";
        out << "  interceptor_start_y: 0\n";
        out << "  interceptor_speed_per_tick: 8\n";
        out << "  intercept_radius: 12\n";
        out << "  engagement_timeout_ticks: 24\n";
        out << "  seeker_fov_deg: 45\n";
        out << "  launch_angle_deg: 45\n";
    }
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "binary",
        .run_forever = true,
        .tick_sleep_ms = 30,
    });
    const auto startup = process::wait_for_startup(server);

    auto console = spawn_command_console(temp_root, startup.tcp_port, "binary");
    const auto [console_exited, console_status] =
        process::wait_for_exit(console.pid, std::chrono::steady_clock::now() + std::chrono::seconds(8));
    assert(console_exited);
    const auto console_lines =
        process::read_remaining_lines_with_deadline(console, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    assert(WIFEXITED(console_status));
    assert(WEXITSTATUS(console_status) == 0);
    console.pid = -1;

    std::ostringstream joined;
    for (const auto& line : console_lines) {
        joined << line << '\n';
    }
    const auto output = joined.str();
    assert(output.find("backend=socket_live") != std::string::npos);
    assert(output.find("session_join: accepted") != std::string::npos);
    assert(output.find("scenario_start: accepted") != std::string::npos);
    assert(output.find("track_request: accepted") != std::string::npos);
    assert(output.find("asset_activate: accepted") != std::string::npos);
    assert(output.find("command_issue: accepted") != std::string::npos);
    assert(output.find("aar_response: cursor=") != std::string::npos);
    assert(output.find("judgment_code=timeout_observed") != std::string::npos);

    ::kill(server.pid, SIGTERM);
    const auto [server_exited, server_status] =
        process::wait_for_exit(server.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(server_exited);
    assert(WIFEXITED(server_status));
    server.pid = -1;

    fs::remove_all(temp_root);
#endif
    return 0;
}
