#include "render_internal.hpp"

#include <array>
#include <cmath>
#include <string_view>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {
namespace {

std::string upper_label(std::string_view text) {
    return uppercase_words(text);
}

std::string connection_value(const RenderContext& ctx) {
    switch (ctx.state.snapshot.display_connection) {
    case icss::core::ConnectionState::Connected:
        return "ON NET";
    case icss::core::ConnectionState::Reconnected:
        return "LINK RESTORED";
    case icss::core::ConnectionState::TimedOut:
        return "TIMED OUT";
    case icss::core::ConnectionState::Disconnected:
        return "OFF NET";
    }
    return "UNKNOWN";
}

std::string picture_status_value(std::string_view freshness) {
    if (freshness == "current") {
        return "CURRENT";
    }
    if (freshness == "degraded") {
        return "DEGRADED";
    }
    if (freshness == "reacquiring") {
        return "REACQUIRING";
    }
    if (freshness == "stale") {
        return "STALE";
    }
    return "UNKNOWN";
}

SDL_Color freshness_color(std::string_view freshness) {
    if (freshness == "current") {
        return rgba(120, 208, 140);
    }
    if (freshness == "degraded") {
        return rgba(255, 189, 89);
    }
    if (freshness == "reacquiring") {
        return rgba(110, 190, 255);
    }
    return rgba(255, 110, 110);
}

std::uint64_t picture_age_ms(const RenderContext& ctx) {
    if (ctx.state.last_datagram_received_ms == 0 || ctx.state.now_ms < ctx.state.last_datagram_received_ms) {
        return 0;
    }
    return ctx.state.now_ms - ctx.state.last_datagram_received_ms;
}

SDL_Color picture_age_color(const RenderContext& ctx, std::uint64_t age_ms) {
    const auto freshness = icss::view::freshness_label(ctx.state.snapshot);
    if (freshness == "stale") {
        return rgba(255, 110, 110);
    }
    if (freshness == "degraded" || age_ms > (ctx.state.snapshot.telemetry.tick > 0 ? 500U : 250U)) {
        return rgba(255, 189, 89);
    }
    if (freshness == "reacquiring") {
        return rgba(110, 190, 255);
    }
    return rgba(120, 208, 140);
}

SDL_Color status_color(std::string_view value) {
    if (value == "pending") {
        return rgba(255, 189, 89);
    }
    if (value == "intercept_success" || value == "complete" || value == "executing" || value == "tracked") {
        return rgba(120, 208, 140);
    }
    if (value == "accepted") {
        return rgba(110, 190, 255);
    }
    if (value == "timeout_observed" || value == "rejected" || value == "invalid_transition") {
        return rgba(255, 110, 110);
    }
    return rgba(188, 198, 214);
}

void render_metric_tile(SDL_Renderer* renderer,
                        const RenderContext& ctx,
                        const SDL_Rect& rect,
                        std::string_view title,
                        const std::string& value,
                        SDL_Color accent,
                        SDL_Color value_color = rgba(236, 239, 244),
                        int value_wrap = 0) {
    fill_panel(renderer, rect, rgba(18, 23, 32), rgba(56, 66, 86));
    SDL_Rect band {rect.x, rect.y, rect.w, 3};
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, accent.a);
    SDL_RenderFillRect(renderer, &band);
    draw_text(renderer, ctx.body_font, rect.x + 10, rect.y + 8, rgba(140, 149, 168), std::string(title));
    draw_text(renderer,
              ctx.body_font,
              rect.x + 10,
              rect.y + 24,
              value_color,
              value,
              value_wrap > 0 ? value_wrap : rect.w - 20);
}

SDL_Color timeline_line_color(std::string_view line, bool aar_mode) {
    if (line.empty()) {
        return rgba(148, 156, 172);
    }
    if (line.rfind("mode=", 0) == 0 || line.rfind("----------------------------------------", 0) == 0) {
        return rgba(126, 201, 126);
    }
    if (aar_mode) {
        if (line.find("Assessment:") != std::string_view::npos) {
            return rgba(255, 189, 89);
        }
        if (line.find("Resilience:") != std::string_view::npos) {
            return rgba(110, 190, 255);
        }
        if (line.find("Current event:") != std::string_view::npos) {
            return rgba(120, 208, 140);
        }
        return rgba(204, 255, 204);
    }
    if (line.find("[control rejected]") != std::string_view::npos || line.find("rejected") != std::string_view::npos) {
        return rgba(255, 136, 136);
    }
    if (line.find("[control accepted]") != std::string_view::npos) {
        return rgba(110, 190, 255);
    }
    if (line.find("Fire order executed") != std::string_view::npos || line.find("Engagement assessed") != std::string_view::npos) {
        return rgba(255, 206, 112);
    }
    if (line.find("[tick ") != std::string_view::npos) {
        return rgba(187, 255, 187);
    }
    return rgba(188, 198, 214);
}

}  // namespace

void render_phase_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.phase_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(141, 211, 199), "Engagement State");
    const std::array<std::pair<icss::core::SessionPhase, const char*>, 8> phase_flow {{
        {icss::core::SessionPhase::Standby, "STANDBY"},
        {icss::core::SessionPhase::Detecting, "DETECTING"},
        {icss::core::SessionPhase::Tracking, "TRACK FILE"},
        {icss::core::SessionPhase::InterceptorReady, "WEAPON READY"},
        {icss::core::SessionPhase::EngageOrdered, "FIRE ORDERED"},
        {icss::core::SessionPhase::Intercepting, "INTERCEPTING"},
        {icss::core::SessionPhase::Assessed, "ENGAGEMENT ASSESSED"},
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
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(255, 179, 102), "Fire Control / Assessment");
    const int gap = 10;
    const int tile_w = (panel.w - 36 - gap) / 2;
    const int tile_h = 50;
    const int top_y = panel.y + 42;
    const int second_y = top_y + tile_h + 10;
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 12, top_y, tile_w, tile_h},
                       "Assessment",
                       upper_label(icss::core::to_string(ctx.state.snapshot.assessment.code)),
                       rgba(255, 179, 102),
                       status_color(icss::core::to_string(ctx.state.snapshot.assessment.code)));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 22 + tile_w, top_y, tile_w, tile_h},
                       "Fire Order",
                       upper_label(icss::core::to_string(ctx.state.snapshot.engage_order_status)),
                       rgba(255, 149, 0),
                       status_color(icss::core::to_string(ctx.state.snapshot.engage_order_status)));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 12, second_y, tile_w, tile_h},
                       "Interceptor",
                       upper_label(icss::core::to_string(ctx.state.snapshot.interceptor_status)),
                       rgba(110, 190, 255),
                       status_color(icss::core::to_string(ctx.state.snapshot.interceptor_status)));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 22 + tile_w, second_y, tile_w, tile_h},
                       "Intercept Mode",
                       ctx.state.effective_track_active ? "TRACKED" : "UNGUIDED",
                       rgba(141, 211, 199),
                       status_color(ctx.state.effective_track_active ? "tracked" : "untracked"));
    const SDL_Rect footer {panel.x + 12, panel.y + panel.h - 58, panel.w - 24, 44};
    const SDL_Color footer_accent = ctx.state.control.last_ok ? rgba(110, 190, 255) : rgba(255, 110, 110);
    render_metric_tile(renderer,
                       ctx,
                       footer,
                       "Latest Console Action",
                       upper_label(ctx.state.control.last_label) + "  |  " + ctx.state.control.last_message,
                       footer_accent,
                       rgba(220, 223, 230),
                       footer.w - 20);
}

void render_resilience_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.resilience_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(129, 199, 255), "Link / Picture Status");
    const int gap = 10;
    const int tile_w = (panel.w - 36 - gap) / 2;
    const int tile_h = 48;
    const int top_y = panel.y + 42;
    const int second_y = top_y + tile_h + 10;
    const auto freshness = icss::view::freshness_label(ctx.state.snapshot);
    const auto age_ms = picture_age_ms(ctx);
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 12, top_y, tile_w, tile_h},
                       "Picture Status",
                       picture_status_value(freshness),
                       freshness_color(freshness),
                       freshness_color(freshness));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 22 + tile_w, top_y, tile_w, tile_h},
                       "Link State",
                       connection_value(ctx),
                       rgba(129, 199, 255),
                       rgba(236, 239, 244));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 12, second_y, tile_w, tile_h},
                       "Picture Age",
                       std::to_string(age_ms) + " ms",
                       picture_age_color(ctx, age_ms),
                       picture_age_color(ctx, age_ms));
    render_metric_tile(renderer,
                       ctx,
                       {panel.x + 22 + tile_w, second_y, tile_w, tile_h},
                       "Loss / Sequence",
                       format_fixed_1(ctx.state.snapshot.telemetry.packet_loss_pct) + " %  |  #"
                           + std::to_string(ctx.state.snapshot.header.snapshot_sequence),
                       rgba(141, 211, 199));
    const SDL_Rect footer {panel.x + 12, panel.y + panel.h - 58, panel.w - 24, 44};
    render_metric_tile(renderer,
                       ctx,
                       footer,
                       "Latest Event",
                       telemetry_event_status(ctx.state),
                       rgba(129, 199, 255),
                       rgba(220, 223, 230),
                       footer.w - 20);
}

void render_entity_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.entity_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(220, 223, 230), "Tactical Picture");
    draw_text(renderer,
              ctx.body_font,
              panel.x + 12,
              panel.y + 30,
              rgba(140, 149, 168),
              "red=target | blue=interceptor | dashed=trail",
              panel.w - 24);
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
    draw_text(renderer, ctx.body_font, panel.x + 12, panel.y + 50, rgba(220, 223, 230), block, panel.w - 24);
}

void render_event_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.event_panel;
    fill_panel(renderer, panel, rgba(8, 10, 14), rgba(54, 60, 78));
    const bool showing_aar = ctx.state.aar.visible && ctx.state.aar.loaded;
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(164, 215, 150), "Event Log");
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
                  timeline_line_color(lines[index], showing_aar),
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
