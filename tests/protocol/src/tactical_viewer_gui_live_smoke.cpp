#include <cassert>
#include <chrono>
#include <cmath>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
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

        const std::string exe = (std::filesystem::path{ICSS_REPO_ROOT} / "build/icss_tactical_display_gui").string();
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
            "--camera-zoom", "1.25",
            "--camera-pan-x", "8",
            "--camera-pan-y", "-6",
            "--headless",
            "--hidden",
            "--auto-control-script",
            "--auto-controls", "target_pos_x_inc,target_pos_y_dec,Start",
            "--duration-ms", "1800",
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
        std::perror("exec icss_tactical_display_gui failed");
        std::_Exit(127);
    }

    ::close(pipe_fds[1]);
    ::fcntl(pipe_fds[0], F_SETFL, ::fcntl(pipe_fds[0], F_GETFL, 0) | O_NONBLOCK);
    return ChildProcess {pid, pipe_fds[0], {}};
}
#endif

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

}  // namespace

int main() {
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    namespace process = icss::testsupport::process;
    using namespace icss::protocol;

    const fs::path temp_root = icss::testsupport::make_temp_configured_repo("icss_gui_viewer_live_");
    auto server = process::spawn_server_process({
        .repo_root = temp_root,
        .tcp_frame_format = "binary",
        .run_forever = true,
        .tick_sleep_ms = 10,
    });
    const auto startup = process::wait_for_startup(server);

    const fs::path dump_path = temp_root / "gui-viewer-state.json";
    auto viewer = spawn_gui_viewer(dump_path, startup.udp_port, startup.tcp_port, 1001U, 201U);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

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
    assert(icss::testsupport::minijson::require_field(object, "schema_version").as_string() == "icss-gui-viewer-state-v1");
    assert(icss::testsupport::minijson::require_field(object, "received_snapshot").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "received_telemetry").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "snapshot_sequence").as_int() >= 1);
    assert(icss::testsupport::minijson::require_field(object, "tick").is_int());
    assert(icss::testsupport::minijson::require_field(object, "phase").as_string() == "detecting");
    assert(icss::testsupport::minijson::require_field(object, "phase_banner").as_string() == "DETECTING");
    assert(icss::testsupport::minijson::require_field(object, "connection_state").as_string() == "connected");
    const auto freshness = icss::testsupport::minijson::require_field(object, "freshness").as_string();
    assert(freshness == "fresh" || freshness == "degraded");
    assert(!icss::testsupport::minijson::require_field(object, "resilience_summary").as_string().empty());
    assert(icss::testsupport::minijson::require_field(object, "assessment_code").as_string() == "pending");
    assert(icss::testsupport::minijson::require_field(object, "heartbeat_count").is_int());
    assert(icss::testsupport::minijson::require_field(object, "last_server_event_tick").is_int());
    assert(icss::testsupport::minijson::require_field(object, "last_server_event_type").as_string() == "session_started");
    assert(icss::testsupport::minijson::require_field(object, "authoritative_headline").as_string().find("SESSION STARTED") != std::string::npos);
    assert(icss::testsupport::minijson::require_field(object, "recommended_control").as_string() == "Track");
    assert(!icss::testsupport::minijson::require_field(object, "target_motion_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "interceptor_motion_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "engagement_visual_visible").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "predicted_marker_visual_visible").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "planned_target_start_x").as_int() == 496);
    assert(icss::testsupport::minijson::require_field(object, "planned_target_start_y").as_int() == 1184);
    assert(icss::testsupport::minijson::require_field(object, "planned_interceptor_start_x").as_int() == 0);
    assert(icss::testsupport::minijson::require_field(object, "planned_interceptor_start_y").as_int() == 0);
    assert(icss::testsupport::minijson::require_field(object, "planned_launch_angle_deg").as_int() == 45);
    assert(icss::testsupport::minijson::require_field(object, "camera_zoom").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_pan_x").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_pan_y").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_visible_min_x").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_visible_max_x").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_visible_min_y").is_number());
    assert(icss::testsupport::minijson::require_field(object, "camera_visible_max_y").is_number());
    assert(icss::testsupport::minijson::require_field(object, "last_control_label").as_string() == "Start");
    assert(icss::testsupport::minijson::require_field(object, "last_control_message").as_string().find("scenario started") != std::string::npos);
    assert(icss::testsupport::minijson::require_field(object, "target_active").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "interceptor_active").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "track_active").as_bool()
        == icss::testsupport::minijson::require_field(object, "track_active").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "target_x").is_int());
    assert(icss::testsupport::minijson::require_field(object, "interceptor_x").is_int());
    const auto actual_interceptor_x = icss::testsupport::minijson::require_field(object, "interceptor_x").as_int();
    const auto actual_interceptor_y = icss::testsupport::minijson::require_field(object, "interceptor_y").as_int();
    assert(actual_interceptor_x == 0);
    assert(actual_interceptor_y == 0);
    assert(icss::testsupport::minijson::require_field(object, "world_width").as_int() == 2304);
    assert(icss::testsupport::minijson::require_field(object, "world_height").as_int() == 1536);
    const auto target_velocity_x = icss::testsupport::minijson::require_field(object, "target_velocity_x").as_int();
    assert(std::abs(target_velocity_x) >= 4 && std::abs(target_velocity_x) <= 6);
    const auto target_velocity_y = icss::testsupport::minijson::require_field(object, "target_velocity_y").as_int();
    assert(std::abs(target_velocity_y) >= 2 && std::abs(target_velocity_y) <= 4);
    assert(icss::testsupport::minijson::require_field(object, "interceptor_speed_per_tick").as_int() == 32);
    assert(icss::testsupport::minijson::require_field(object, "interceptor_acceleration_per_tick").is_number());
    assert(icss::testsupport::minijson::require_field(object, "active_launch_angle_deg").is_number());
    assert(icss::testsupport::minijson::require_field(object, "seeker_fov_deg").is_number());
    assert(icss::testsupport::minijson::require_field(object, "target_world_x").is_number());
    assert(icss::testsupport::minijson::require_field(object, "time_to_intercept_s").is_number());
    assert(!icss::testsupport::minijson::require_field(object, "predicted_intercept_valid").as_bool());
    assert(!icss::testsupport::minijson::require_field(object, "seeker_lock").as_bool());
    assert(icss::testsupport::minijson::require_field(object, "track_measurement_residual_distance").is_number());
    assert(icss::testsupport::minijson::require_field(object, "track_covariance_trace").is_number());
    assert(icss::testsupport::minijson::require_field(object, "recent_event_count").as_int() >= 2);

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
