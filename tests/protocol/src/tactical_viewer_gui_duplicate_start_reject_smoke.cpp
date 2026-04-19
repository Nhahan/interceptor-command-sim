#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "icss/core/config.hpp"
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

int clamp_int(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

icss::core::ScenarioConfig randomize_like_gui(const icss::core::ScenarioConfig& base, std::uint64_t seed) {
    std::mt19937 rng(static_cast<std::uint32_t>(seed ^ 0x9E3779B97F4A7C15ULL));
    auto jitter = [&](int value, int min_value, int max_value, int spread) {
        std::uniform_int_distribution<int> dist(-spread, spread);
        return clamp_int(value + dist(rng), min_value, max_value);
    };

    auto randomized = base;
    randomized.target_start_x = jitter(base.target_start_x, 0, base.world_width - 1, 24);
    randomized.target_start_y = jitter(base.target_start_y, 0, base.world_height - 1, 24);
    randomized.target_velocity_x = jitter(base.target_velocity_x, -12, 12, 1);
    randomized.target_velocity_y = jitter(base.target_velocity_y, -12, 12, 1);

    if (randomized.target_velocity_x == 0 && randomized.target_velocity_y == 0) {
        randomized.target_velocity_x = base.target_velocity_x == 0 ? 1 : (base.target_velocity_x > 0 ? 1 : -1);
    }
    if (randomized.target_start_x == base.target_start_x
        && randomized.target_start_y == base.target_start_y
        && randomized.target_velocity_x == base.target_velocity_x
        && randomized.target_velocity_y == base.target_velocity_y) {
        randomized.target_start_x = clamp_int(base.target_start_x + 16, 0, base.world_width - 1);
    }
    return randomized;
}

ChildProcess spawn_gui_viewer(const std::filesystem::path& dump_path,
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
            "--auto-controls", "Start,Start",
            "--duration-ms", "320",
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

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_duplicate_start_");
    const auto runtime = icss::core::load_runtime_config(temp_root);
    const auto expected_first = randomize_like_gui(runtime.scenario, 1U);
    const auto expected_second = randomize_like_gui(runtime.scenario, 2U);

    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "binary",
        .run_forever = true,
        .tick_sleep_ms = 500,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "gui-duplicate-start-state.json";
    auto viewer = spawn_gui_viewer(dump_path, startup.udp_port, startup.tcp_port, 1001U, 201U);

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

    const auto dump_json = icss::testsupport::minijson::parse(read_text(dump_path));
    assert(dump_json.is_object());
    const auto& object = dump_json.as_object();
    assert(icss::testsupport::minijson::require_field(object, "phase").as_string() == "detecting");
    assert(icss::testsupport::minijson::require_field(object, "last_control_label").as_string() == "Start");
    const auto last_message = icss::testsupport::minijson::require_field(object, "last_control_message").as_string();
    assert(last_message.find("initialized state") != std::string::npos);

    const auto target_x = icss::testsupport::minijson::require_field(object, "target_x").as_int();
    const auto target_y = icss::testsupport::minijson::require_field(object, "target_y").as_int();
    const auto asset_x = icss::testsupport::minijson::require_field(object, "asset_x").as_int();
    const auto asset_y = icss::testsupport::minijson::require_field(object, "asset_y").as_int();
    const auto target_vx = icss::testsupport::minijson::require_field(object, "target_velocity_x").as_int();
    const auto target_vy = icss::testsupport::minijson::require_field(object, "target_velocity_y").as_int();

    assert(target_x == expected_first.target_start_x);
    assert(target_y == expected_first.target_start_y);
    assert(asset_x == expected_first.interceptor_start_x);
    assert(asset_y == expected_first.interceptor_start_y);
    assert(target_vx == expected_first.target_velocity_x);
    assert(target_vy == expected_first.target_velocity_y);

    assert(target_x != expected_second.target_start_x
           || target_y != expected_second.target_start_y
           || asset_x != expected_second.interceptor_start_x
           || asset_y != expected_second.interceptor_start_y
           || target_vx != expected_second.target_velocity_x
           || target_vy != expected_second.target_velocity_y);

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
