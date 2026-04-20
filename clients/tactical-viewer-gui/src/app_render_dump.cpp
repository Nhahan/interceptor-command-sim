#include "app.hpp"
#include "render_internal.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {
namespace {

const char* effective_track_state(const ViewerState& state) {
    return state.effective_track_active ? "tracked" : "untracked";
}

const char* intercept_profile(const ViewerState& state) {
    return state.effective_track_active ? "tracked_intercept" : "unguided_intercept";
}

}  // namespace

void write_dump_state(const std::filesystem::path& path, const ViewerState& state, const ViewerOptions& options) {
    if (path.empty()) {
        return;
    }
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path);
    const auto transform = make_viewport_transform(build_layout(options).map_rect, state, options);
    out << "{\n";
    out << "  \"schema_version\": \"icss-gui-viewer-state-v1\",\n";
    out << "  \"received_snapshot\": " << (state.received_snapshot ? "true" : "false") << ",\n";
    out << "  \"received_telemetry\": " << (state.received_telemetry ? "true" : "false") << ",\n";
    out << "  \"now_ms\": " << state.now_ms << ",\n";
    out << "  \"last_datagram_received_ms\": " << state.last_datagram_received_ms << ",\n";
    out << "  \"picture_age_ms\": " << ((state.last_datagram_received_ms > 0 && state.now_ms >= state.last_datagram_received_ms)
        ? (state.now_ms - state.last_datagram_received_ms)
        : 0) << ",\n";
    out << "  \"snapshot_sequence\": " << state.snapshot.header.snapshot_sequence << ",\n";
    out << "  \"tick\": " << state.snapshot.telemetry.tick << ",\n";
    out << "  \"phase\": \"" << icss::core::to_string(state.snapshot.phase) << "\",\n";
    out << "  \"phase_banner\": \"" << escape_json(phase_banner_label(state.snapshot.phase)) << "\",\n";
    out << "  \"connection_state\": \"" << icss::core::to_string(state.snapshot.display_connection) << "\",\n";
    out << "  \"picture_status\": \"" << icss::view::freshness_label(state.snapshot) << "\",\n";
    out << "  \"resilience_summary\": \"" << escape_json(resilience_summary(state)) << "\",\n";
    out << "  \"assessment_code\": \"" << icss::core::to_string(state.snapshot.assessment.code) << "\",\n";
    out << "  \"heartbeat_count\": " << state.heartbeat_count << ",\n";
    out << "  \"last_server_event_tick\": " << state.last_server_event_tick << ",\n";
    out << "  \"last_server_event_type\": \"" << escape_json(state.last_server_event_type) << "\",\n";
    out << "  \"last_server_event_summary\": \"" << escape_json(state.last_server_event_summary) << "\",\n";
    out << "  \"timeline_scroll_lines\": " << state.timeline_scroll_lines << ",\n";
    out << "  \"authoritative_headline\": \"" << escape_json(authoritative_headline(state)) << "\",\n";
    out << "  \"recommended_control\": \"" << escape_json(recommended_control_label(state)) << "\",\n";
    out << "  \"target_motion_visual_visible\": " << (target_motion_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"interceptor_motion_visual_visible\": " << (interceptor_motion_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"engagement_visual_visible\": " << (engagement_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"predicted_marker_visual_visible\": " << (predicted_marker_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"planned_target_start_x\": " << state.planned_scenario.target_start_x << ",\n";
    out << "  \"planned_target_start_y\": " << state.planned_scenario.target_start_y << ",\n";
    out << "  \"planned_target_velocity_x\": " << state.planned_scenario.target_velocity_x << ",\n";
    out << "  \"planned_target_velocity_y\": " << state.planned_scenario.target_velocity_y << ",\n";
    out << "  \"planned_interceptor_start_x\": " << state.planned_scenario.interceptor_start_x << ",\n";
    out << "  \"planned_interceptor_start_y\": " << state.planned_scenario.interceptor_start_y << ",\n";
    out << "  \"planned_interceptor_speed_per_tick\": " << state.planned_scenario.interceptor_speed_per_tick << ",\n";
    out << "  \"planned_engagement_timeout_ticks\": " << state.planned_scenario.engagement_timeout_ticks << ",\n";
    out << "  \"planned_launch_angle_deg\": " << state.planned_scenario.launch_angle_deg << ",\n";
    out << "  \"camera_zoom\": " << options.camera_zoom << ",\n";
    out << "  \"camera_pan_x\": " << options.camera_pan_x << ",\n";
    out << "  \"camera_pan_y\": " << options.camera_pan_y << ",\n";
    out << "  \"camera_visible_min_x\": " << transform.visible_min_x << ",\n";
    out << "  \"camera_visible_min_y\": " << transform.visible_min_y << ",\n";
    out << "  \"camera_visible_max_x\": " << transform.visible_max_x << ",\n";
    out << "  \"camera_visible_max_y\": " << transform.visible_max_y << ",\n";
    out << "  \"aar_available\": " << (aar_available(state) ? "true" : "false") << ",\n";
    out << "  \"aar_loaded\": " << (state.aar.loaded ? "true" : "false") << ",\n";
    out << "  \"aar_visible\": " << (state.aar.visible ? "true" : "false") << ",\n";
    out << "  \"aar_cursor_index\": " << state.aar.cursor_index << ",\n";
    out << "  \"aar_total_events\": " << state.aar.total_events << ",\n";
    out << "  \"aar_assessment_code\": \"" << escape_json(state.aar.assessment_code) << "\",\n";
    out << "  \"aar_resilience_case\": \"" << escape_json(state.aar.resilience_case) << "\",\n";
    out << "  \"aar_event_type\": \"" << escape_json(state.aar.event_type) << "\",\n";
    out << "  \"aar_event_summary\": \"" << escape_json(state.aar.event_summary) << "\",\n";
    out << "  \"last_control_label\": \"" << escape_json(state.control.last_label) << "\",\n";
    out << "  \"last_control_message\": \"" << escape_json(state.control.last_message) << "\",\n";
    out << "  \"start_enabled\": " << (control_button_enabled("Start", state) ? "true" : "false") << ",\n";
    out << "  \"track_enabled\": " << (control_button_enabled("Track", state) ? "true" : "false") << ",\n";
    out << "  \"ready_enabled\": " << (control_button_enabled("Ready", state) ? "true" : "false") << ",\n";
    out << "  \"engage_enabled\": " << (control_button_enabled("Engage", state) ? "true" : "false") << ",\n";
    out << "  \"reset_enabled\": " << (control_button_enabled("Reset", state) ? "true" : "false") << ",\n";
    out << "  \"aar_enabled\": " << (control_button_enabled("AAR", state) ? "true" : "false") << ",\n";
    out << "  \"interceptor_status\": \"" << icss::core::to_string(state.snapshot.interceptor_status) << "\",\n";
    out << "  \"engage_order_status\": \"" << icss::core::to_string(state.snapshot.engage_order_status) << "\",\n";
    out << "  \"command_visual_active\": " << (command_visual_active(state) ? "true" : "false") << ",\n";
    out << "  \"target_active\": " << (state.snapshot.target.active ? "true" : "false") << ",\n";
    out << "  \"interceptor_active\": " << (state.snapshot.interceptor.active ? "true" : "false") << ",\n";
    out << "  \"effective_track_state\": \"" << effective_track_state(state) << "\",\n";
    out << "  \"intercept_profile\": \"" << intercept_profile(state) << "\",\n";
    out << "  \"track_active\": " << (state.snapshot.track.active ? "true" : "false") << ",\n";
    out << "  \"track_measurement_residual_distance\": " << state.snapshot.track.measurement_residual_distance << ",\n";
    out << "  \"track_covariance_trace\": " << state.snapshot.track.covariance_trace << ",\n";
    out << "  \"track_measurement_valid\": " << (state.snapshot.track.measurement_valid ? "true" : "false") << ",\n";
    out << "  \"track_estimated_x\": " << state.snapshot.track.estimated_position.x << ",\n";
    out << "  \"track_estimated_y\": " << state.snapshot.track.estimated_position.y << ",\n";
    out << "  \"track_measurement_x\": " << state.snapshot.track.measurement_position.x << ",\n";
    out << "  \"track_measurement_y\": " << state.snapshot.track.measurement_position.y << ",\n";
    out << "  \"track_measurement_age_ticks\": " << state.snapshot.track.measurement_age_ticks << ",\n";
    out << "  \"track_missed_updates\": " << state.snapshot.track.missed_updates << ",\n";
    out << "  \"target_x\": " << state.snapshot.target.position.x << ",\n";
    out << "  \"target_y\": " << state.snapshot.target.position.y << ",\n";
    out << "  \"target_velocity_x\": " << state.snapshot.target_velocity_x << ",\n";
    out << "  \"target_velocity_y\": " << state.snapshot.target_velocity_y << ",\n";
    out << "  \"target_world_x\": " << state.snapshot.target_world_position.x << ",\n";
    out << "  \"target_world_y\": " << state.snapshot.target_world_position.y << ",\n";
    out << "  \"target_heading_deg\": " << state.snapshot.target_heading_deg << ",\n";
    out << "  \"interceptor_x\": " << state.snapshot.interceptor.position.x << ",\n";
    out << "  \"interceptor_y\": " << state.snapshot.interceptor.position.y << ",\n";
    out << "  \"interceptor_world_x\": " << state.snapshot.interceptor_world_position.x << ",\n";
    out << "  \"interceptor_world_y\": " << state.snapshot.interceptor_world_position.y << ",\n";
    out << "  \"interceptor_heading_deg\": " << state.snapshot.interceptor_heading_deg << ",\n";
    out << "  \"world_width\": " << state.snapshot.world_width << ",\n";
    out << "  \"world_height\": " << state.snapshot.world_height << ",\n";
    out << "  \"interceptor_speed_per_tick\": " << state.snapshot.interceptor_speed_per_tick << ",\n";
    out << "  \"interceptor_acceleration_per_tick\": " << state.snapshot.interceptor_acceleration_per_tick << ",\n";
    out << "  \"engagement_timeout_ticks\": " << state.snapshot.engagement_timeout_ticks << ",\n";
    out << "  \"active_launch_angle_deg\": " << state.snapshot.launch_angle_deg << ",\n";
    out << "  \"seeker_fov_deg\": " << state.snapshot.seeker_fov_deg << ",\n";
    out << "  \"seeker_lock\": " << (state.snapshot.seeker_lock ? "true" : "false") << ",\n";
    out << "  \"off_boresight_deg\": " << state.snapshot.off_boresight_deg << ",\n";
    out << "  \"predicted_intercept_valid\": " << (state.snapshot.predicted_intercept_valid ? "true" : "false") << ",\n";
    out << "  \"predicted_intercept_x\": " << state.snapshot.predicted_intercept_position.x << ",\n";
    out << "  \"predicted_intercept_y\": " << state.snapshot.predicted_intercept_position.y << ",\n";
    out << "  \"time_to_intercept_s\": " << state.snapshot.time_to_intercept_s << ",\n";
    out << "  \"recent_event_count\": " << state.recent_server_events.size() << "\n";
    out << "}\n";
}

void write_dump_golden_state(const std::filesystem::path& path, const ViewerState& state) {
    if (path.empty()) {
        return;
    }
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream out(path);
    out << "{\n";
    out << "  \"schema_version\": \"icss-gui-viewer-golden-state-v1\",\n";
    out << "  \"phase\": \"" << icss::core::to_string(state.snapshot.phase) << "\",\n";
    out << "  \"phase_banner\": \"" << escape_json(phase_banner_label(state.snapshot.phase)) << "\",\n";
    out << "  \"assessment_code\": \"" << icss::core::to_string(state.snapshot.assessment.code) << "\",\n";
    out << "  \"effective_track_state\": \"" << effective_track_state(state) << "\",\n";
    out << "  \"intercept_profile\": \"" << intercept_profile(state) << "\",\n";
    out << "  \"recommended_control\": \"" << escape_json(recommended_control_label(state)) << "\",\n";
    out << "  \"interceptor_status\": \"" << icss::core::to_string(state.snapshot.interceptor_status) << "\",\n";
    out << "  \"engage_order_status\": \"" << icss::core::to_string(state.snapshot.engage_order_status) << "\",\n";
    out << "  \"target_active\": " << (state.snapshot.target.active ? "true" : "false") << ",\n";
    out << "  \"interceptor_active\": " << (state.snapshot.interceptor.active ? "true" : "false") << ",\n";
    out << "  \"planned_launch_angle_deg\": " << state.planned_scenario.launch_angle_deg << ",\n";
    out << "  \"active_launch_angle_deg\": " << state.snapshot.launch_angle_deg << ",\n";
    out << "  \"target_motion_visual_visible\": " << (target_motion_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"interceptor_motion_visual_visible\": " << (interceptor_motion_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"engagement_visual_visible\": " << (engagement_visual_visible(state) ? "true" : "false") << ",\n";
    out << "  \"predicted_marker_visual_visible\": " << (predicted_marker_visual_visible(state) ? "true" : "false") << "\n";
    out << "}\n";
}

void write_dump_frame(SDL_Renderer* renderer, const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }
    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        throw std::runtime_error("failed to create frame dump surface");
    }
    if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) != 0) {
        SDL_FreeSurface(surface);
        throw std::runtime_error("failed to read renderer pixels");
    }
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    if (SDL_SaveBMP(surface, path.string().c_str()) != 0) {
        SDL_FreeSurface(surface);
        throw std::runtime_error("failed to save viewer frame dump");
    }
    SDL_FreeSurface(surface);
}

}  // namespace icss::viewer_gui
