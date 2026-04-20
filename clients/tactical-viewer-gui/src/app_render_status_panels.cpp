#include "render_internal.hpp"

#include <array>
#include <cmath>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {

void render_phase_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.phase_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(141, 211, 199), "Mission Phase");
    const std::array<std::pair<icss::core::SessionPhase, const char*>, 8> phase_flow {{
        {icss::core::SessionPhase::Standby, "STANDBY"},
        {icss::core::SessionPhase::Detecting, "DETECTING"},
        {icss::core::SessionPhase::Tracking, "TRACK ESTABLISHED"},
        {icss::core::SessionPhase::InterceptorReady, "INTERCEPTOR READY"},
        {icss::core::SessionPhase::EngageOrdered, "ENGAGE ORDERED"},
        {icss::core::SessionPhase::Intercepting, "INTERCEPTING"},
        {icss::core::SessionPhase::Assessed, "ASSESSMENT COMPLETE"},
        {icss::core::SessionPhase::Archived, "ARCHIVED"},
    }};
    for (std::size_t index = 0; index < phase_flow.size(); ++index) {
        const SDL_Rect row {panel.x + 12, panel.y + 42 + static_cast<int>(index) * 20, panel.w - 24, 18};
        const auto row_phase = phase_flow[index].first;
        const auto active = row_phase == ctx.state.snapshot.phase;
        const auto completed = static_cast<int>(row_phase) < static_cast<int>(ctx.state.snapshot.phase);
        if (active) {
            SDL_SetRenderDrawColor(renderer, ctx.phase_color.r, ctx.phase_color.g, ctx.phase_color.b, 255);
            SDL_RenderFillRect(renderer, &row);
            draw_text(renderer, ctx.body_font, row.x + 8, row.y + 1, rgba(10, 12, 16), phase_flow[index].second);
        } else {
            SDL_SetRenderDrawColor(renderer, completed ? 68 : 42, completed ? 91 : 50, completed ? 72 : 58, 255);
            SDL_RenderFillRect(renderer, &row);
            SDL_SetRenderDrawColor(renderer, 66, 74, 92, 255);
            SDL_RenderDrawRect(renderer, &row);
            draw_text(renderer,
                      ctx.body_font,
                      row.x + 8,
                      row.y + 1,
                      completed ? rgba(176, 216, 184) : rgba(148, 156, 172),
                      phase_flow[index].second);
        }
    }
}

void render_decision_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.decision_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(255, 179, 102), "Assessment / Orders");
    std::string block;
    block += std::string(ctx.state.control.last_ok ? "Action: accepted | " : "Action: rejected | ")
        + ctx.state.control.last_label + " | " + ctx.state.control.last_message + "\n";
    block += "Assessment: " + std::string(icss::core::to_string(ctx.state.snapshot.assessment.code)) + "\n";
    block += "Engage order: " + std::string(icss::core::to_string(ctx.state.snapshot.engage_order_status)) + "\n";
    block += "Interceptor status: " + std::string(icss::core::to_string(ctx.state.snapshot.interceptor_status));
    draw_text(renderer, ctx.body_font, panel.x + 12, panel.y + 44, rgba(220, 223, 230), block, panel.w - 24);
}

void render_resilience_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.resilience_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(129, 199, 255), "Resilience / Telemetry");
    std::string block;
    block += "Connection: " + std::string(icss::core::to_string(ctx.state.snapshot.display_connection))
        + " | Freshness: " + icss::view::freshness_label(ctx.state.snapshot) + "\n";
    block += "Tick: " + std::to_string(ctx.state.snapshot.telemetry.tick)
        + " | Snapshot: " + std::to_string(ctx.state.snapshot.header.snapshot_sequence) + "\n";
    block += "Last snapshot: " + std::to_string(ctx.state.snapshot.header.timestamp_ms) + " ms\n";
    block += "Link Delay: " + format_fixed_0(ctx.state.snapshot.telemetry.latency_ms)
        + " ms | Loss: " + format_fixed_1(ctx.state.snapshot.telemetry.packet_loss_pct) + " %";
    block += ctx.state.snapshot.telemetry.packet_loss_pct > 0.0F ? " | degraded" : " | stable";
    draw_text(renderer, ctx.body_font, panel.x + 12, panel.y + 44, rgba(220, 223, 230), block, panel.w - 24);
}

void render_entity_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.entity_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(220, 223, 230), "Tactical Display");
    draw_text(renderer,
              ctx.body_font,
              panel.x + 150,
              panel.y + 14,
              rgba(140, 149, 168),
              "red=target | blue=interceptor | dashed=trail",
              panel.w - 162);
    std::string block;
    block += "T=(" + std::to_string(static_cast<int>(std::lround(ctx.state.snapshot.target_world_position.x)))
        + "," + std::to_string(static_cast<int>(std::lround(ctx.state.snapshot.target_world_position.y))) + ") "
        + (ctx.state.snapshot.target.active ? "yes" : "no")
        + " | I=(" + std::to_string(static_cast<int>(std::lround(ctx.state.snapshot.interceptor_world_position.x)))
        + "," + std::to_string(static_cast<int>(std::lround(ctx.state.snapshot.interceptor_world_position.y))) + ") "
        + (ctx.state.snapshot.interceptor.active ? "yes" : "no") + "\n";
    block += "Tv=(" + format_fixed_1(ctx.state.snapshot.target_velocity.x) + "," + format_fixed_1(ctx.state.snapshot.target_velocity.y)
        + ") h=" + format_fixed_1(ctx.state.snapshot.target_heading_deg)
        + " | Iv=(" + format_fixed_1(ctx.state.snapshot.interceptor_velocity.x) + "," + format_fixed_1(ctx.state.snapshot.interceptor_velocity.y)
        + ") h=" + format_fixed_1(ctx.state.snapshot.interceptor_heading_deg) + "\n";
    block += "Track " + std::string(ctx.state.snapshot.track.active ? "tracked" : "untracked")
        + " | residual="
        + (ctx.state.snapshot.track.measurement_valid ? format_fixed_1(ctx.state.snapshot.track.measurement_residual_distance) : std::string("n/a"))
        + " | cov=" + format_fixed_1(ctx.state.snapshot.track.covariance_trace) + "\n";
    block += "age=" + std::to_string(ctx.state.snapshot.track.measurement_age_ticks)
        + " | misses=" + std::to_string(ctx.state.snapshot.track.missed_updates)
        + " | launch=" + format_fixed_1(ctx.state.snapshot.launch_angle_deg) + " deg"
        + " | TTI=" + (ctx.state.snapshot.predicted_intercept_valid ? format_fixed_1(ctx.state.snapshot.time_to_intercept_s) + "s" : std::string("pending"))
        + " | seeker=" + (ctx.state.snapshot.predicted_intercept_valid ? (ctx.state.snapshot.seeker_lock ? "yes" : "no") : "n/a");
    draw_text(renderer, ctx.body_font, panel.x + 12, panel.y + 42, rgba(220, 223, 230), block, panel.w - 24);
}

void render_event_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.event_panel;
    fill_panel(renderer, panel, rgba(8, 10, 14), rgba(54, 60, 78));
    const bool showing_aar = ctx.state.aar.visible && ctx.state.aar.loaded;
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(164, 215, 150), "Event Timeline");
    draw_text(renderer,
              ctx.body_font,
              panel.x + 190,
              panel.y + 14,
              rgba(120, 128, 144),
              showing_aar ? "[aar mode]" : "[live mode]");
    auto lines = terminal_timeline_lines(ctx.state, showing_aar);
    const int line_height = std::max(16, TTF_FontLineSkip(ctx.body_font));
    const int scrollbar_w = 10;
    const SDL_Rect body_rect {panel.x + 12, panel.y + 46, panel.w - 36, panel.h - 58};
    const std::size_t visible_lines = std::max(1, body_rect.h / line_height);
    const std::size_t total_lines = lines.size();
    const std::size_t max_scroll = total_lines > visible_lines ? (total_lines - visible_lines) : 0;
    const std::size_t scroll = std::min(ctx.state.timeline_scroll_lines, max_scroll);
    const SDL_Rect text_clip {body_rect.x, body_rect.y, body_rect.w - scrollbar_w - 8, body_rect.h};
    SDL_RenderSetClipRect(renderer, &text_clip);
    const std::size_t end = std::min(total_lines, scroll + visible_lines);
    for (std::size_t index = scroll; index < end; ++index) {
        draw_text(renderer,
                  ctx.body_font,
                  body_rect.x,
                  body_rect.y + static_cast<int>((index - scroll) * line_height),
                  rgba(187, 255, 187),
                  lines[index]);
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    if (total_lines > visible_lines) {
        const SDL_Rect track {panel.x + panel.w - 18, body_rect.y, scrollbar_w, body_rect.h};
        SDL_SetRenderDrawColor(renderer, 28, 34, 46, 220);
        SDL_RenderFillRect(renderer, &track);
        SDL_SetRenderDrawColor(renderer, 54, 60, 78, 255);
        SDL_RenderDrawRect(renderer, &track);
        const int thumb_h = std::max(24, static_cast<int>((static_cast<float>(visible_lines) / static_cast<float>(total_lines)) * static_cast<float>(track.h)));
        const float denom = max_scroll == 0 ? 1.0F : static_cast<float>(max_scroll);
        const float thumb_ratio = static_cast<float>(scroll) / denom;
        const int thumb_y = track.y + static_cast<int>(thumb_ratio * static_cast<float>(track.h - thumb_h));
        SDL_Rect thumb {track.x + 1, thumb_y, track.w - 2, thumb_h};
        SDL_SetRenderDrawColor(renderer, 112, 192, 112, 230);
        SDL_RenderFillRect(renderer, &thumb);
        SDL_SetRenderDrawColor(renderer, 164, 215, 150, 255);
        SDL_RenderDrawRect(renderer, &thumb);
    }
}

}  // namespace icss::viewer_gui
