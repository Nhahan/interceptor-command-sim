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
            "--auto-controls", "Start,Track,Activate,Command",
            "--duration-ms", "1400",
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

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_command_feedback_");
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "json",
        .run_forever = true,
        .tick_sleep_ms = 20,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "viewer-command-feedback.json";
    auto viewer = spawn_gui_viewer(dump_path, startup.udp_port, startup.tcp_port);

    const auto [viewer_exited, viewer_status] =
        process::wait_for_exit(viewer.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(viewer_exited);
    assert(WIFEXITED(viewer_status));
    assert(WEXITSTATUS(viewer_status) == 0);
    viewer.pid = -1;

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_path));
    assert(dump_json.is_object());
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "received_snapshot").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "received_telemetry").as_bool());
    const auto phase = icss::testsupport::minijson::require_field(object, "phase").as_string();
    assert(phase == "command_issued" || phase == "engaging" || phase == "judged");
    const auto phase_banner = icss::testsupport::minijson::require_field(object, "phase_banner").as_string();
    assert(phase_banner == "COMMAND ACCEPTED" || phase_banner == "ENGAGING" || phase_banner == "JUDGMENT PRODUCED");
    assert(icss::testsupport::minijson::require_field(object, "target_active").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "asset_active").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "last_control_label").as_string() == "Command");
    assert(icss::testsupport::minijson::require_field(object, "last_control_message").as_string().find("command accepted") != std::string::npos);
    const auto authoritative_headline = icss::testsupport::minijson::require_field(object, "authoritative_headline").as_string();
    assert(authoritative_headline.find("ACTIVE COMMAND") != std::string::npos
           || authoritative_headline.find("JUDGMENT") != std::string::npos);
    assert(icss::testsupport::minijson::require_field(object, "recommended_control").as_string() == "Stop");
    assert(icss::testsupport::minijson::require_field(object, "last_server_event_tick").is_int());
    const auto command_status = icss::testsupport::minijson::require_field(object, "command_status").as_string();
    assert(command_status == "accepted" || command_status == "executing" || command_status == "completed");
    const auto asset_status = icss::testsupport::minijson::require_field(object, "asset_status").as_string();
    assert(asset_status == "engaging" || asset_status == "complete");
    assert(icss::testsupport::minijson::require_field(object, "command_visual_active").as_bool() || command_status == "completed");
    assert(icss::testsupport::minijson::require_field(object, "recent_event_count").as_int() >= 4);

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
