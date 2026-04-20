#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

#if !defined(_WIN32)
struct SocketGuard {
    int fd {-1};
    explicit SocketGuard(int value = -1) : fd(value) {}
    ~SocketGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

void set_timeout(int fd) {
    timeval timeout {};
    timeout.tv_sec = 0;
    timeout.tv_usec = 300000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void send_binary_frame(int fd, std::string_view kind, std::string_view payload) {
    const auto frame = icss::protocol::encode_binary_frame(kind, payload);
    ::send(fd, frame.data(), frame.size(), 0);
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

icss::protocol::FramedMessage wait_for_binary_frame(icss::net::TransportBackend& backend, int fd, int attempts = 8) {
    std::string buffer;
    for (int i = 0; i < attempts; ++i) {
        backend.poll_once();
        const auto bytes = recv_some(fd);
        buffer.append(bytes.begin(), bytes.end());
        icss::protocol::FramedMessage frame;
        if (icss::protocol::try_decode_binary_frame(buffer, frame)) {
            return frame;
        }
    }
    return {};
}

std::vector<std::string> recv_udp_messages(int fd, std::size_t max_messages) {
    std::vector<std::string> messages;
    char buffer[4096];
    sockaddr_in from {};
    for (std::size_t i = 0; i < max_messages; ++i) {
        socklen_t len = sizeof(from);
        const auto received = ::recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &len);
        if (received <= 0) {
            break;
        }
        messages.emplace_back(buffer, buffer + received);
    }
    return messages;
}
#endif

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;
    using namespace icss::net;
    using namespace icss::protocol;

    auto config = default_runtime_config(fs::path{ICSS_REPO_ROOT});

    auto inproc = make_transport(BackendKind::InProcess, config);
    assert(inproc->backend_name() == "in_process");
    inproc->connect_client(ClientRole::FireControlConsole, 101U);
    inproc->connect_client(ClientRole::TacticalDisplay, 201U);
    const auto bad_session = inproc->dispatch(TrackAcquirePayload{{9999U, 101U, 1U}, "target-alpha"});
    assert(!bad_session.accepted);
    const auto bad_target = inproc->dispatch(TrackAcquirePayload{{1001U, 101U, 1U}, "wrong-target"});
    assert(!bad_target.accepted);
    assert(inproc->start_scenario().accepted);
    assert(inproc->dispatch(TrackAcquirePayload{{1001U, 101U, 1U}, "target-alpha"}).accepted);
    assert(inproc->dispatch(TrackDropPayload{{1001U, 101U, 2U}, "target-alpha"}).accepted);
    assert(inproc->latest_snapshot().phase == SessionPhase::Detecting);
    assert(inproc->dispatch(TrackAcquirePayload{{1001U, 101U, 3U}, "target-alpha"}).accepted);
    assert(inproc->dispatch(InterceptorReadyPayload{{1001U, 101U, 2U}, "interceptor-alpha"}).accepted);
    assert(inproc->dispatch(TrackDropPayload{{1001U, 101U, 4U}, "target-alpha"}).accepted);
    assert(inproc->latest_snapshot().phase == SessionPhase::InterceptorReady);
    assert(!inproc->latest_snapshot().track.active);
    assert(inproc->dispatch(TrackAcquirePayload{{1001U, 101U, 5U}, "target-alpha"}).accepted);
    assert(inproc->latest_snapshot().phase == SessionPhase::InterceptorReady);
    assert(inproc->latest_snapshot().track.active);
    assert(inproc->dispatch(EngageOrderPayload{{1001U, 101U, 6U}, "interceptor-alpha", "target-alpha"}).accepted);
    for (int i = 0; i < config.scenario.engagement_timeout_ticks && !inproc->latest_snapshot().assessment.ready; ++i) {
        inproc->advance_tick();
    }
    inproc->archive_session();
    const auto inproc_summary = inproc->summary();
    assert(inproc_summary.assessment_ready);
    assert(inproc_summary.assessment_code == AssessmentCode::InterceptSuccess);

    auto inproc_disconnect = make_transport(BackendKind::InProcess, config);
    inproc_disconnect->connect_client(ClientRole::FireControlConsole, 101U);
    inproc_disconnect->disconnect_client(ClientRole::FireControlConsole, "fire control console test disconnect");
    const auto disconnected_sender = inproc_disconnect->dispatch(EngageOrderPayload{{1001U, 101U, 4U}, "interceptor-alpha", "target-alpha"});
    assert(!disconnected_sender.accepted);

#if !defined(_WIN32)
    const auto unique_suffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path temp_root = fs::temp_directory_path() / ("icss_socket_backend_test_" + unique_suffix);
    fs::remove_all(temp_root);
    fs::create_directories(temp_root / "configs");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/server.example.yaml", temp_root / "configs/server.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/scenario.example.yaml", temp_root / "configs/scenario.example.yaml");
    fs::copy_file(fs::path{ICSS_REPO_ROOT} / "configs/logging.example.yaml", temp_root / "configs/logging.example.yaml");

    auto live_config = default_runtime_config(temp_root);
    live_config.server.tcp_port = 0;
    live_config.server.udp_port = 0;
    live_config.server.tcp_frame_format = "binary";
    auto live = make_transport(BackendKind::SocketLive, live_config);
    assert(live->backend_name() == "socket_live");
    const auto info = live->info();
    assert(info.binds_network);
    assert(info.tcp_port > 0);
    assert(info.udp_port > 0);

    SocketGuard tcp_client(::socket(AF_INET, SOCK_STREAM, 0));
    SocketGuard udp_viewer(::socket(AF_INET, SOCK_DGRAM, 0));
    assert(tcp_client.fd >= 0);
    assert(udp_viewer.fd >= 0);
    set_timeout(tcp_client.fd);
    set_timeout(udp_viewer.fd);

    sockaddr_in tcp_addr {};
    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_port = htons(info.tcp_port);
    assert(::inet_pton(AF_INET, live_config.server.bind_host.c_str(), &tcp_addr.sin_addr) == 1);
    assert(::connect(tcp_client.fd, reinterpret_cast<sockaddr*>(&tcp_addr), sizeof(tcp_addr)) == 0);

    sockaddr_in udp_bind_addr {};
    udp_bind_addr.sin_family = AF_INET;
    udp_bind_addr.sin_port = htons(0);
    assert(::inet_pton(AF_INET, live_config.server.bind_host.c_str(), &udp_bind_addr.sin_addr) == 1);
    assert(::bind(udp_viewer.fd, reinterpret_cast<sockaddr*>(&udp_bind_addr), sizeof(udp_bind_addr)) == 0);

    sockaddr_in udp_server_addr {};
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(info.udp_port);
    assert(::inet_pton(AF_INET, live_config.server.bind_host.c_str(), &udp_server_addr.sin_addr) == 1);

    const auto viewer_join = serialize(SessionJoinPayload{{1001U, 201U, 1U}, "tactical_display"});
    const std::string malformed_udp = "malformed_datagram";
    assert(::sendto(udp_viewer.fd, malformed_udp.data(), malformed_udp.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);
    assert(::sendto(udp_viewer.fd, viewer_join.data(), viewer_join.size(), 0,
                    reinterpret_cast<sockaddr*>(&udp_server_addr), sizeof(udp_server_addr)) >= 0);

    const auto bad_frame = encode_binary_frame("unsupported_kind", "payload");
    assert(::send(tcp_client.fd, bad_frame.data(), bad_frame.size(), 0) >= 0);
    send_binary_frame(tcp_client.fd, "session_join", serialize(SessionJoinPayload{{1001U, 101U, 1U}, "fire_control_console"}));
    const auto join_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(join_frame.kind == "command_ack");
    const auto join_ack = parse_command_ack(join_frame.payload);
    assert(join_ack.accepted);

    send_binary_frame(tcp_client.fd, "scenario_start", serialize(ScenarioStartPayload{{1001U, 101U, 2U}, live_config.scenario.name}));
    const auto start_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(start_frame.kind == "command_ack");
    const auto start_ack = parse_command_ack(start_frame.payload);
    assert(start_ack.accepted);

    send_binary_frame(tcp_client.fd,
                      "scenario_start",
                      serialize(ScenarioStartPayload{{1001U, 101U, 22U},
                                                     live_config.scenario.name,
                                                     320,
                                                     240,
                                                     999,
                                                     999,
                                                     1,
                                                     1,
                                                     1,
                                                     1,
                                                     8,
                                                     10,
                                                     10,
                                                     30}));
    const auto duplicate_start_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(duplicate_start_frame.kind == "command_ack");
    const auto duplicate_start_ack = parse_command_ack(duplicate_start_frame.payload);
    assert(!duplicate_start_ack.accepted);
    assert(duplicate_start_ack.reason.find("standby state") != std::string::npos);
    const auto snapshot_after_duplicate_start = live->latest_snapshot();
    assert(snapshot_after_duplicate_start.world_width != 320);
    assert(snapshot_after_duplicate_start.world_height != 240);
    assert(snapshot_after_duplicate_start.target_world_position.x != 999.0F);
    assert(snapshot_after_duplicate_start.target_world_position.y != 999.0F);

    send_binary_frame(tcp_client.fd, "track_acquire", serialize(TrackAcquirePayload{{1001U, 101U, 3U}, "wrong-target"}));
    const auto bad_track_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(bad_track_frame.kind == "command_ack");
    const auto bad_track_ack = parse_command_ack(bad_track_frame.payload);
    assert(!bad_track_ack.accepted);

    send_binary_frame(tcp_client.fd, "track_acquire", serialize(TrackAcquirePayload{{1001U, 101U, 4U}, "target-alpha"}));
    const auto track_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(track_frame.kind == "command_ack");
    const auto track_ack = parse_command_ack(track_frame.payload);
    assert(track_ack.accepted);

    live->advance_tick();

    send_binary_frame(tcp_client.fd, "interceptor_ready", serialize(InterceptorReadyPayload{{1001U, 101U, 5U}, "interceptor-alpha"}));
    const auto asset_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(asset_frame.kind == "command_ack");
    const auto asset_ack = parse_command_ack(asset_frame.payload);
    assert(asset_ack.accepted);

    send_binary_frame(tcp_client.fd, "track_drop", serialize(TrackDropPayload{{1001U, 101U, 6U}, "target-alpha"}));
    const auto release_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(release_frame.kind == "command_ack");
    const auto release_ack = parse_command_ack(release_frame.payload);
    assert(release_ack.accepted);
    const auto interceptor_ready_snapshot = live->latest_snapshot();
    assert(interceptor_ready_snapshot.phase == SessionPhase::InterceptorReady);
    assert(!interceptor_ready_snapshot.track.active);

    send_binary_frame(tcp_client.fd, "engage_order", serialize(EngageOrderPayload{{1001U, 101U, 7U}, "interceptor-alpha", "target-alpha"}));
    const auto command_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(command_frame.kind == "command_ack");
    const auto command_ack = parse_command_ack(command_frame.payload);
    assert(command_ack.accepted);

    (void)recv_udp_messages(udp_viewer.fd, 128);
    send_binary_frame(tcp_client.fd, "track_drop", serialize(TrackDropPayload{{1001U, 101U, 8U}, "target-alpha"}));
    const auto late_release_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(late_release_frame.kind == "command_ack");
    const auto late_release_ack = parse_command_ack(late_release_frame.payload);
    assert(!late_release_ack.accepted);
    assert(late_release_ack.reason.find("after engage order") != std::string::npos);

    live->advance_tick();
    {
        const auto degraded_snapshot = live->latest_snapshot();
        assert(degraded_snapshot.launch_angle_deg == static_cast<float>(live_config.scenario.launch_angle_deg));
        assert(!degraded_snapshot.predicted_intercept_valid);
        assert(degraded_snapshot.interceptor_heading_deg > 40.0F && degraded_snapshot.interceptor_heading_deg < 50.0F);
        assert(degraded_snapshot.telemetry.packet_loss_pct > 0.0F);
        const auto degraded_frame = icss::view::render_tactical_frame(
            degraded_snapshot,
            live->events(),
            icss::view::make_replay_cursor(live->events().size(), live->events().empty() ? 0 : live->events().size() - 1));
        assert(degraded_frame.find("picture_status=degraded") != std::string::npos);
        assert(degraded_frame.find("packet_loss_pct=25.0") != std::string::npos);
    }
    live->advance_tick();

    send_binary_frame(tcp_client.fd, "scenario_stop", serialize(ScenarioStopPayload{{1001U, 101U, 9U}, "scenario stop requested"}));
    const auto stop_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(stop_frame.kind == "command_ack");
    const auto stop_ack = parse_command_ack(stop_frame.payload);
    assert(stop_ack.accepted);

    send_binary_frame(tcp_client.fd, "scenario_stop", serialize(ScenarioStopPayload{{1001U, 101U, 23U}, "duplicate scenario stop"}));
    const auto duplicate_stop_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(duplicate_stop_frame.kind == "command_ack");
    const auto duplicate_stop_ack = parse_command_ack(duplicate_stop_frame.payload);
    assert(!duplicate_stop_ack.accepted);
    assert(duplicate_stop_ack.reason.find("active scenario is running") != std::string::npos);

    send_binary_frame(tcp_client.fd, "interceptor_ready", serialize(InterceptorReadyPayload{{1001U, 101U, 24U}, "interceptor-alpha"}));
    const auto post_stop_asset_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(post_stop_asset_frame.kind == "command_ack");
    const auto post_stop_asset_ack = parse_command_ack(post_stop_asset_frame.payload);
    assert(!post_stop_asset_ack.accepted);
    assert(post_stop_asset_ack.reason.find("only valid while tracking") != std::string::npos);

    send_binary_frame(tcp_client.fd, "engage_order", serialize(EngageOrderPayload{{1001U, 101U, 25U}, "interceptor-alpha", "target-alpha"}));
    const auto post_stop_command_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(post_stop_command_frame.kind == "command_ack");
    const auto post_stop_command_ack = parse_command_ack(post_stop_command_frame.payload);
    assert(!post_stop_command_ack.accepted);
    assert(post_stop_command_ack.reason.find("only valid while interceptor_ready") != std::string::npos);

    send_binary_frame(tcp_client.fd, "scenario_reset", serialize(ScenarioResetPayload{{1001U, 101U, 26U}, "reset after archive"}));
    const auto reset_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(reset_frame.kind == "command_ack");
    const auto reset_ack = parse_command_ack(reset_frame.payload);
    assert(reset_ack.accepted);
    assert(reset_ack.reason.find("reset") != std::string::npos);

    send_binary_frame(tcp_client.fd, "scenario_start", serialize(ScenarioStartPayload{{1001U, 101U, 27U}, live_config.scenario.name}));
    const auto restart_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(restart_frame.kind == "command_ack");
    const auto restart_ack = parse_command_ack(restart_frame.payload);
    assert(restart_ack.accepted);

    send_binary_frame(tcp_client.fd, "track_acquire", serialize(TrackAcquirePayload{{1001U, 101U, 28U}, "target-alpha"}));
    const auto restart_track_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(restart_track_frame.kind == "command_ack");
    const auto restart_track_ack = parse_command_ack(restart_track_frame.payload);
    assert(restart_track_ack.accepted);

    send_binary_frame(tcp_client.fd, "aar_request", serialize(AarRequestPayload{{1001U, 101U, 29U}, 999U}));
    const auto aar_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(aar_frame.kind == "aar_response");
    const auto aar_response = parse_aar_response(aar_frame.payload);
    assert(aar_response.total_events >= 1U);
    assert(aar_response.replay_cursor_index == aar_response.total_events - 1U);
    assert(aar_response.control == "absolute");
    assert(aar_response.requested_index == 999U);
    assert(aar_response.clamped);
    assert(aar_response.assessment_code == "pending");
    assert(!aar_response.event_type.empty());
    assert(!aar_response.event_summary.empty());

    send_binary_frame(tcp_client.fd, "aar_request", serialize(AarRequestPayload{{1001U, 101U, 30U}, 999U, "step_backward"}));
    const auto aar_prev_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(aar_prev_frame.kind == "aar_response");
    const auto aar_prev = parse_aar_response(aar_prev_frame.payload);
    assert(aar_prev.replay_cursor_index + 1 == aar_response.replay_cursor_index);
    assert(aar_prev.control == "step_backward");
    assert(aar_prev.requested_index == 999U);
    assert(!aar_prev.clamped);

    send_binary_frame(tcp_client.fd, "aar_request", serialize(AarRequestPayload{{1001U, 101U, 31U}, 0U, "step_forward"}));
    const auto aar_next_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(aar_next_frame.kind == "aar_response");
    const auto aar_next = parse_aar_response(aar_next_frame.payload);
    assert(aar_next.replay_cursor_index == aar_response.replay_cursor_index);
    assert(aar_next.control == "step_forward");
    assert(!aar_next.clamped);

    send_binary_frame(tcp_client.fd, "aar_request", serialize(AarRequestPayload{{1001U, 101U, 32U}, 0U, "step_forward"}));
    const auto aar_next_clamped_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(aar_next_clamped_frame.kind == "aar_response");
    const auto aar_next_clamped = parse_aar_response(aar_next_clamped_frame.payload);
    assert(aar_next_clamped.replay_cursor_index == aar_next.replay_cursor_index);
    assert(aar_next_clamped.control == "step_forward");
    assert(aar_next_clamped.clamped);

    send_binary_frame(tcp_client.fd,
                      "aar_request",
                      "kind=aar_request;session_id=1001;sender_id=101;sequence=33;replay_cursor_index=0;control=rewind");
    const auto bad_aar_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(bad_aar_frame.kind == "command_ack");
    const auto bad_aar_ack = parse_command_ack(bad_aar_frame.payload);
    assert(!bad_aar_ack.accepted);
    assert(bad_aar_ack.reason.find("aar_request rejected") != std::string::npos);

    send_binary_frame(tcp_client.fd, "session_leave", serialize(SessionLeavePayload{{1001U, 101U, 34U}, "operator leaving session"}));
    const auto leave_frame = wait_for_binary_frame(*live, tcp_client.fd);
    assert(leave_frame.kind == "command_ack");
    const auto leave_ack = parse_command_ack(leave_frame.payload);
    assert(leave_ack.accepted);

    const auto udp_messages = recv_udp_messages(udp_viewer.fd, 40);
    bool saw_snapshot = false;
    bool saw_telemetry = false;
    for (const auto& wire : udp_messages) {
        if (wire.rfind("kind=world_snapshot", 0) == 0) {
            const auto payload = parse_snapshot(wire);
            assert(payload.target_id == "target-alpha");
            saw_snapshot = true;
        }
        if (wire.rfind("kind=telemetry", 0) == 0) {
            const auto payload = parse_telemetry(wire);
            assert(!payload.connection_state.empty());
            saw_telemetry = true;
        }
    }
    assert(saw_snapshot);
    assert(saw_telemetry);
    fs::remove_all(temp_root);
#else
    auto live = make_transport(BackendKind::SocketLive, config);
    const auto info = live->info();
    assert(!info.binds_network);
#endif
    return 0;
}
