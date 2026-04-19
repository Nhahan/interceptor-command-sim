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

ChildProcess spawn_gui_viewer(const std::filesystem::path& dump_path,
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
            "--host", "127.0.0.1",
            "--udp-port", udp,
            "--tcp-port", tcp,
            "--headless",
            "--hidden",
            "--auto-controls", "Start,Guidance,target_pos_x_inc,target_pos_x_inc",
            "--duration-ms", "1600",
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

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_setup_next_start_");
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "json",
        .run_forever = true,
        .tick_sleep_ms = 20,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "viewer-setup-next-start.json";
    auto viewer = spawn_gui_viewer(dump_path, startup.udp_port, startup.tcp_port);

    const auto [viewer_exited, viewer_status] =
        process::wait_for_exit(viewer.pid, std::chrono::steady_clock::now() + std::chrono::seconds(6));
    assert(viewer_exited);
    assert(WIFEXITED(viewer_status));
    assert(WEXITSTATUS(viewer_status) == 0);
    viewer.pid = -1;

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_path));
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "phase").as_string() == "tracking");
    assert(icss::testsupport::minijson::require_field(object, "planned_target_start_x").as_int() == 512);
    const auto target_world_x = icss::testsupport::minijson::require_field(object, "target_world_x").as_double();
    assert(target_world_x != 512.0);
    assert(icss::testsupport::minijson::require_field(object, "last_control_label").as_string() == "Setup");
    assert(icss::testsupport::minijson::require_field(object, "last_control_message").as_string().find("target_start_x") != std::string::npos);

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
