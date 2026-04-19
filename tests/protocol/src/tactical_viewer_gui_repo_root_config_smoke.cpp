#include <cassert>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>

#include "icss/support/minijson.hpp"
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

ChildProcess spawn_gui_viewer(const std::filesystem::path& repo_root,
                              const std::filesystem::path& dump_path,
                              std::uint16_t udp_port,
                              std::uint16_t tcp_port) {
    int pipe_fds[2] {};
    assert(::pipe(pipe_fds) == 0);
    const auto pid = ::fork();
    assert(pid >= 0);
    if (pid == 0) {
        ::close(pipe_fds[0]);
        ::dup2(pipe_fds[1], STDOUT_FILENO);
        ::dup2(pipe_fds[1], STDERR_FILENO);
        ::close(pipe_fds[1]);

        ::setenv("SDL_VIDEODRIVER", "dummy", 1);

        const std::string exe = (std::filesystem::path{ICSS_REPO_ROOT} / "build/icss_tactical_viewer_gui").string();
        const std::string udp = std::to_string(udp_port);
        const std::string tcp = std::to_string(tcp_port);
        std::vector<std::string> argv_storage {
            exe,
            "--repo-root", repo_root.string(),
            "--host", "127.0.0.1",
            "--udp-port", udp,
            "--tcp-port", tcp,
            "--headless",
            "--hidden",
            "--duration-ms", "900",
            "--heartbeat-interval-ms", "100",
            "--dump-state", dump_path.string(),
        };
        std::vector<char*> argv;
        argv.reserve(argv_storage.size() + 1);
        for (auto& value : argv_storage) {
            argv.push_back(value.data());
        }
        argv.push_back(nullptr);
        ::execv(exe.c_str(), argv.data());
        std::perror("exec icss_tactical_viewer_gui failed");
        std::_Exit(127);
    }

    ::close(pipe_fds[1]);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFL, 0) | O_NONBLOCK);
    return ChildProcess {pid, pipe_fds[0], {}};
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}
#endif

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    namespace process = icss::testsupport::process;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_repo_root_config_");
    {
        std::ofstream out(temp_root / "configs/scenario.example.yaml");
        out << "scenario:\n";
        out << "  name: basic_intercept_training\n";
        out << "  description: viewer config preload verification\n";
        out << "entities:\n";
        out << "  targets: 1\n";
        out << "  assets: 1\n";
        out << "rules:\n";
        out << "  enable_replay: true\n";
        out << "  telemetry_interval_ms: 200\n";
        out << "  world_width: 640\n";
        out << "  world_height: 320\n";
        out << "  target_start_x: 96\n";
        out << "  target_start_y: 288\n";
        out << "  target_velocity_x: 7\n";
        out << "  target_velocity_y: -4\n";
        out << "  interceptor_start_x: 0\n";
        out << "  interceptor_start_y: 0\n";
        out << "  interceptor_speed_per_tick: 40\n";
        out << "  intercept_radius: 20\n";
        out << "  engagement_timeout_ticks: 70\n";
        out << "  seeker_fov_deg: 50\n";
        out << "  launch_angle_deg: 50\n";
    }

    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "json",
        .run_forever = true,
        .tick_sleep_ms = 20,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "viewer-repo-root-config.json";
    auto viewer = spawn_gui_viewer(temp_root, dump_path, startup.udp_port, startup.tcp_port);

    const auto [viewer_exited, viewer_status] =
        process::wait_for_exit(viewer.pid, std::chrono::steady_clock::now() + std::chrono::seconds(4));
    assert(viewer_exited);
    assert(WIFEXITED(viewer_status));
    assert(WEXITSTATUS(viewer_status) == 0);
    viewer.pid = -1;

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_path));
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "planned_target_start_x").as_int() == 96);
    assert(icss::testsupport::minijson::require_field(object, "planned_target_start_y").as_int() == 288);
    assert(icss::testsupport::minijson::require_field(object, "planned_target_velocity_x").as_int() == 7);
    assert(icss::testsupport::minijson::require_field(object, "planned_interceptor_start_x").as_int() == 0);
    assert(icss::testsupport::minijson::require_field(object, "planned_interceptor_speed_per_tick").as_int() == 40);
    assert(icss::testsupport::minijson::require_field(object, "planned_engagement_timeout_ticks").as_int() == 70);
    assert(icss::testsupport::minijson::require_field(object, "planned_launch_angle_deg").as_int() == 50);
    assert(icss::testsupport::minijson::require_field(object, "world_width").as_int() == 640);
    assert(icss::testsupport::minijson::require_field(object, "world_height").as_int() == 320);

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
