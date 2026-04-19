#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

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

ChildProcess spawn_gui_viewer_relative(const std::filesystem::path& cwd,
                                       std::uint16_t udp_port,
                                       std::uint16_t tcp_port,
                                       std::uint32_t session_id,
                                       std::uint32_t sender_id) {
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
        const auto rc = ::chdir(cwd.c_str());
        assert(rc == 0);

        const std::string exe = (std::filesystem::path {ICSS_REPO_ROOT} / "build/icss_tactical_viewer_gui").string();
        const std::string port = std::to_string(udp_port);
        const std::string tcp = std::to_string(tcp_port);
        const std::string session = std::to_string(session_id);
        const std::string sender = std::to_string(sender_id);
        std::vector<std::string> argv_storage {
            exe,
            "--host", "127.0.0.1",
            "--udp-port", port,
            "--tcp-port", tcp,
            "--tcp-frame-format", "binary",
            "--session-id", session,
            "--sender-id", sender,
            "--headless",
            "--hidden",
            "--auto-control-script",
            "--auto-controls", "Start",
            "--duration-ms", "1500",
            "--heartbeat-interval-ms", "100",
            "--dump-state", "gui-relative-state.json",
            "--dump-frame", "gui-relative-frame.bmp",
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
#endif

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    namespace process = icss::testsupport::process;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_viewer_relative_dump_");
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "binary",
        .run_forever = true,
        .tick_sleep_ms = 10,
    });
    const auto startup = process::wait_for_startup(server);

    auto viewer = spawn_gui_viewer_relative(temp_root, startup.udp_port, startup.tcp_port, 1001U, 201U);
    const auto [viewer_exited, viewer_status] =
        process::wait_for_exit(viewer.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(viewer_exited);
    const auto viewer_lines =
        process::read_remaining_lines_with_deadline(viewer, std::chrono::steady_clock::now() + std::chrono::seconds(1));
    if (!WIFEXITED(viewer_status) || WEXITSTATUS(viewer_status) != 0) {
        for (const auto& line : viewer_lines) {
            std::fprintf(stderr, "viewer: %s\n", line.c_str());
        }
    }
    assert(WIFEXITED(viewer_status));
    assert(WEXITSTATUS(viewer_status) == 0);
    viewer.pid = -1;

    const auto dump_state_path = temp_root / "gui-relative-state.json";
    const auto dump_frame_path = temp_root / "gui-relative-frame.bmp";
    assert(fs::exists(dump_state_path));
    assert(fs::exists(dump_frame_path));

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_state_path));
    assert(dump_json.is_object());
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "received_snapshot").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "received_telemetry").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "timeline_scroll_lines").is_int());

    ::kill(server.pid, SIGTERM);
    const auto [server_exited, server_status] =
        process::wait_for_exit(server.pid, std::chrono::steady_clock::now() + std::chrono::seconds(5));
    assert(server_exited);
    assert(WIFEXITED(server_status));
    assert(WEXITSTATUS(server_status) == 0);
    server.pid = -1;

    fs::remove_all(temp_root);
#endif
    return 0;
}
