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
            "--auto-controls", "asset_speed_dec,asset_speed_dec,asset_speed_dec,timeout_dec,timeout_dec,timeout_dec,timeout_dec,timeout_dec,Start,Guidance,Activate,Command",
            "--duration-ms", "2200",
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

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_parameter_timeout_");
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "json",
        .run_forever = true,
        .tick_sleep_ms = 20,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "viewer-parameter-timeout.json";
    auto viewer = spawn_gui_viewer(dump_path, startup.udp_port, startup.tcp_port);

    const auto [viewer_exited, viewer_status] =
        process::wait_for_exit(viewer.pid, std::chrono::steady_clock::now() + std::chrono::seconds(6));
    assert(viewer_exited);
    assert(WIFEXITED(viewer_status));
    assert(WEXITSTATUS(viewer_status) == 0);
    viewer.pid = -1;

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_path));
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "planned_interceptor_speed_per_tick").as_int() == 8);
    assert(icss::testsupport::minijson::require_field(object, "planned_engagement_timeout_ticks").as_int() == 10);
    assert(icss::testsupport::minijson::require_field(object, "interceptor_speed_per_tick").as_int() == 8);
    assert(icss::testsupport::minijson::require_field(object, "engagement_timeout_ticks").as_int() == 10);
    assert(icss::testsupport::minijson::require_field(object, "judgment_code").as_string() == "timeout_observed");
    const auto phase = icss::testsupport::minijson::require_field(object, "phase").as_string();
    assert(phase == "judged" || phase == "archived");
    assert(!icss::testsupport::minijson::require_field(object, "asset_motion_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "engagement_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "predicted_marker_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "command_visual_active").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "seeker_lock").as_bool() == false
           || icss::testsupport::minijson::require_field(object, "off_boresight_deg").as_double() >= 0.0);

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
