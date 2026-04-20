#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "icss/core/config.hpp"
#include "icss/core/types.hpp"
#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/payloads.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

namespace icss::viewer_gui {

struct ViewerOptions {
    std::string host {"127.0.0.1"};
    std::uint16_t udp_port {4001};
    std::uint16_t tcp_port {4000};
    std::uint32_t session_id {1001U};
    std::uint32_t sender_id {201U};
    std::uint64_t duration_ms {0};
    std::uint64_t heartbeat_interval_ms {100};
    std::filesystem::path repo_root {std::filesystem::current_path()};
    float camera_zoom {1.0F};
    float camera_pan_x {0.0F};
    float camera_pan_y {0.0F};
    int width {1360};
    int height {860};
    bool hidden {false};
    bool headless {false};
    bool auto_control_script {false};
    std::string tcp_frame_format {"json"};
    std::vector<std::string> auto_controls;
    std::filesystem::path dump_state_path;
    std::filesystem::path dump_frame_path;
    std::filesystem::path dump_golden_state_path;
    std::filesystem::path font_path;
};

struct ControlState {
    bool connected {false};
    bool last_ok {false};
    std::string last_label {"idle"};
    std::string last_message {"control panel idle"};
    std::uint64_t sequence {1};
    std::uint64_t start_seed {1};
    std::size_t auto_step {0};
    std::uint64_t auto_last_action_ms {0};
};

struct AarState {
    bool available {false};
    bool loaded {false};
    bool visible {false};
    std::uint64_t cursor_index {0};
    std::uint64_t total_events {0};
    std::string assessment_code {"pending"};
    std::string resilience_case {"none"};
    std::string event_type {"none"};
    std::string event_summary {"aar not requested"};
};

struct ViewerState {
    icss::core::Snapshot snapshot {};
    icss::core::ScenarioConfig planned_scenario {};
    bool received_snapshot {false};
    bool received_telemetry {false};
    std::deque<std::string> recent_server_events;
    std::uint64_t heartbeat_id {0};
    std::uint64_t heartbeat_count {0};
    std::uint64_t snapshot_count_received {0};
    std::uint64_t telemetry_count_received {0};
    std::uint64_t last_datagram_received_ms {0};
    std::uint64_t last_join_attempt_ms {0};
    std::uint64_t last_server_event_tick {0};
    std::size_t timeline_scroll_lines {0};
    std::string last_server_event_type {"none"};
    std::string last_server_event_summary {"no server event"};
    std::deque<icss::core::Vec2f> target_history;
    std::deque<icss::core::Vec2f> interceptor_history;
    bool effective_track_active {false};
    ControlState control;
    AarState aar;
};

struct ViewportTransform {
    SDL_FRect screen_bounds {};
    int world_width {1};
    int world_height {1};
    float visible_min_x {0.0F};
    float visible_min_y {0.0F};
    float visible_max_x {0.0F};
    float visible_max_y {0.0F};
    float pixels_per_world_x {1.0F};
    float pixels_per_world_y {1.0F};
};

struct Button {
    std::string action;
    std::string label;
    SDL_Rect rect {};
};

struct GuiLayout {
    SDL_Rect header_panel {};
    SDL_Rect phase_strip {};
    SDL_Rect map_rect {};
    SDL_Rect entity_panel {};
    int panel_x {0};
    SDL_Rect phase_panel {};
    SDL_Rect decision_panel {};
    SDL_Rect resilience_panel {};
    SDL_Rect control_panel {};
    SDL_Rect setup_panel {};
    SDL_Rect event_panel {};
    std::vector<Button> buttons;
};

void push_timeline_entry(ViewerState& state, std::string message);
std::string escape_json(std::string_view input);
ViewerOptions parse_args(int argc, char** argv);

icss::core::InterceptorStatus parse_interceptor_status(std::string_view value);
icss::core::EngageOrderStatus parse_engage_order_status(std::string_view value);
icss::core::AssessmentCode parse_assessment_code(std::string_view value);
icss::core::ConnectionState parse_connection_state(std::string_view value);
icss::core::SessionPhase parse_phase(std::string_view value);
std::string uppercase_words(std::string_view value);
std::string phase_banner_label(icss::core::SessionPhase phase);
std::string phase_operator_note(icss::core::SessionPhase phase);
SDL_Color phase_accent(icss::core::SessionPhase phase);
std::string resilience_summary(const ViewerState& state);
std::string authoritative_headline(const ViewerState& state);
std::string authoritative_badge_label(const ViewerState& state);
SDL_Color authoritative_badge_color(const ViewerState& state);
bool aar_available(const ViewerState& state);
std::string aar_panel_text(const ViewerState& state);
std::string control_display_label(std::string_view action, const ViewerState& state);
std::string terminal_timeline_text(const ViewerState& state, bool aar_mode);
std::vector<std::string> terminal_timeline_lines(const ViewerState& state, bool aar_mode);
std::string control_timeline_message(std::string_view label, bool ok, std::string_view message);
std::string latest_timeline_entry(const ViewerState& state);
std::string telemetry_event_status(const ViewerState& state);
std::string format_fixed_1(float value);
std::string format_fixed_0(std::uint32_t value);
float heading_deg_gui(icss::core::Vec2f v);
icss::core::ScenarioConfig default_viewer_scenario(const std::filesystem::path& repo_root);
bool apply_parameter_action(ViewerState& state, std::string_view action);
bool is_live_control_action(std::string_view action);
std::string recommended_control_label(const ViewerState& state);
void sync_preview_from_scenario(ViewerState& state, const icss::core::ScenarioConfig& scenario);
icss::core::ScenarioConfig randomize_start_scenario(const icss::core::ScenarioConfig& base, std::uint64_t seed);
bool target_motion_visual_visible(const ViewerState& state);
bool interceptor_motion_visual_visible(const ViewerState& state);
bool engagement_visual_visible(const ViewerState& state);
bool predicted_marker_visual_visible(const ViewerState& state);
bool command_visual_active(const ViewerState& state);
void sync_preview_from_planned_scenario(ViewerState& state);
void apply_snapshot(ViewerState& state, const icss::protocol::SnapshotPayload& payload);
void apply_telemetry(ViewerState& state, const icss::protocol::TelemetryPayload& payload);

#if !defined(_WIN32)
enum class FrameMode {
    Json,
    Binary,
};

class UdpSocket {
public:
    explicit UdpSocket(const std::string& host);
    ~UdpSocket();
    [[nodiscard]] int get() const;
private:
    int fd_ {-1};
};

class TcpSocket {
public:
    TcpSocket() = default;
    ~TcpSocket();
    void connect_to(const std::string& host, std::uint16_t port);
    void reset();
    [[nodiscard]] bool connected() const;
    [[nodiscard]] int fd() const;
private:
    int fd_ {-1};
};

FrameMode parse_frame_mode(std::string_view value);
void send_viewer_join(UdpSocket& socket, const ViewerOptions& options, std::uint64_t& sequence);
void send_display_heartbeat(UdpSocket& socket, const ViewerOptions& options, std::uint64_t& sequence, ViewerState& state);
void receive_datagrams(UdpSocket& socket, const ViewerOptions& options, ViewerState& state, std::uint64_t now_ms);
void send_frame(TcpSocket& socket, FrameMode mode, std::string_view kind, std::string_view payload);
icss::protocol::FramedMessage recv_frame(TcpSocket& socket, FrameMode mode);
void ensure_control_connected(TcpSocket& socket, ViewerState& state, const ViewerOptions& options, FrameMode mode);
void perform_control_action(std::string_view action_label,
                            ViewerState& state,
                            const ViewerOptions& options,
                            FrameMode frame_mode,
                            TcpSocket& control_socket);
#endif

SDL_Color rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255);
void fill_panel(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color fill, SDL_Color border);
void draw_text(SDL_Renderer* renderer,
               TTF_Font* font,
               int x,
               int y,
               SDL_Color color,
               const std::string& text,
               int wrap_width = 0);
GuiLayout build_layout(const ViewerOptions& options);
void render_gui(SDL_Renderer* renderer,
                TTF_Font* title_font,
                TTF_Font* body_font,
                const ViewerState& state,
                const ViewerOptions& options);
void write_dump_frame(SDL_Renderer* renderer, const std::filesystem::path& path);
void write_dump_state(const std::filesystem::path& path, const ViewerState& state, const ViewerOptions& options);
void write_dump_golden_state(const std::filesystem::path& path, const ViewerState& state);

}  // namespace icss::viewer_gui
