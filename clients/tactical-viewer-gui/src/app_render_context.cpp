#include "render_internal.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kCameraEpsilon = 0.001F;
constexpr std::size_t kLiveAutoFrameHistoryPoints = 18;

bool has_manual_camera_override(const icss::viewer_gui::ViewerOptions& options) {
    return options.camera_zoom > 1.001F
        || std::abs(options.camera_pan_x) > kCameraEpsilon
        || std::abs(options.camera_pan_y) > kCameraEpsilon;
}

bool should_auto_frame(const icss::viewer_gui::ViewerState& state,
                       const icss::viewer_gui::ViewerOptions& options) {
    if (has_manual_camera_override(options)) {
        return false;
    }
    using Phase = icss::core::SessionPhase;
    switch (state.snapshot.phase) {
    case Phase::Detecting:
    case Phase::Tracking:
    case Phase::InterceptorReady:
    case Phase::EngageOrdered:
    case Phase::Intercepting:
    case Phase::Assessed:
    case Phase::Archived:
        return state.received_snapshot;
    case Phase::Standby:
        return false;
    }
    return false;
}

bool use_full_history_for_frame(const icss::viewer_gui::ViewerState& state) {
    return state.snapshot.phase == icss::core::SessionPhase::Assessed
        || state.snapshot.phase == icss::core::SessionPhase::Archived
        || state.aar.visible;
}

void include_point(float& min_x,
                   float& min_y,
                   float& max_x,
                   float& max_y,
                   icss::core::Vec2f point) {
    min_x = std::min(min_x, point.x);
    min_y = std::min(min_y, point.y);
    max_x = std::max(max_x, point.x);
    max_y = std::max(max_y, point.y);
}

}  // namespace

namespace icss::viewer_gui {

ViewportTransform make_viewport_transform(const SDL_Rect& map_rect,
                                         const ViewerState& state,
                                         const ViewerOptions& options) {
    ViewportTransform transform;
    transform.screen_bounds = SDL_FRect {
        static_cast<float>(map_rect.x),
        static_cast<float>(map_rect.y),
        static_cast<float>(map_rect.w),
        static_cast<float>(map_rect.h),
    };
    transform.world_width = std::max(state.snapshot.world_width, 1);
    transform.world_height = std::max(state.snapshot.world_height, 1);
    const float full_span_x = static_cast<float>(std::max(transform.world_width - 1, 1));
    const float full_span_y = static_cast<float>(std::max(transform.world_height - 1, 1));
    const float full_center_x = full_span_x * 0.5F;
    const float full_center_y = full_span_y * 0.5F;

    if (should_auto_frame(state, options)) {
        float min_x = full_span_x;
        float min_y = full_span_y;
        float max_x = 0.0F;
        float max_y = 0.0F;
        bool has_points = false;
        auto add_point = [&](icss::core::Vec2f point) {
            include_point(min_x, min_y, max_x, max_y, point);
            has_points = true;
        };

        if (state.snapshot.phase != icss::core::SessionPhase::Intercepting
            && state.snapshot.phase != icss::core::SessionPhase::Assessed
            && state.snapshot.phase != icss::core::SessionPhase::Archived) {
            add_point({0.0F, 0.0F});
        }
        add_point(state.snapshot.target_world_position);
        if (state.snapshot.interceptor.active
            || state.snapshot.phase == icss::core::SessionPhase::InterceptorReady
            || state.snapshot.phase == icss::core::SessionPhase::EngageOrdered
            || state.snapshot.phase == icss::core::SessionPhase::Intercepting
            || state.snapshot.phase == icss::core::SessionPhase::Assessed
            || state.snapshot.phase == icss::core::SessionPhase::Archived) {
            add_point(state.snapshot.interceptor_world_position);
        }
        if (state.snapshot.predicted_intercept_valid) {
            add_point(state.snapshot.predicted_intercept_position);
        }
        const auto include_history_tail = [&](const std::deque<icss::core::Vec2f>& history) {
            const auto start = use_full_history_for_frame(state) || history.size() <= kLiveAutoFrameHistoryPoints
                ? std::size_t {0}
                : history.size() - kLiveAutoFrameHistoryPoints;
            for (std::size_t index = start; index < history.size(); ++index) {
                add_point(history[index]);
            }
        };
        include_history_tail(state.target_history);
        include_history_tail(state.interceptor_history);

        if (has_points) {
            const float map_aspect = transform.screen_bounds.w / std::max(1.0F, transform.screen_bounds.h);
            float span_x = std::max(320.0F, (max_x - min_x) + 180.0F);
            float span_y = std::max(220.0F, (max_y - min_y) + 180.0F);
            const float span_aspect = span_x / std::max(1.0F, span_y);
            if (span_aspect > map_aspect) {
                span_y = span_x / map_aspect;
            } else {
                span_x = span_y * map_aspect;
            }
            span_x = std::min(span_x, full_span_x);
            span_y = std::min(span_y, full_span_y);
            const float center_x = std::clamp((min_x + max_x) * 0.5F, span_x * 0.5F, full_span_x - span_x * 0.5F);
            const float center_y = std::clamp((min_y + max_y) * 0.5F, span_y * 0.5F, full_span_y - span_y * 0.5F);
            transform.visible_min_x = std::max(0.0F, center_x - span_x * 0.5F);
            transform.visible_max_x = std::min(full_span_x, center_x + span_x * 0.5F);
            transform.visible_min_y = std::max(0.0F, center_y - span_y * 0.5F);
            transform.visible_max_y = std::min(full_span_y, center_y + span_y * 0.5F);
        } else {
            transform.visible_min_x = 0.0F;
            transform.visible_max_x = full_span_x;
            transform.visible_min_y = 0.0F;
            transform.visible_max_y = full_span_y;
        }
    } else {
        const float zoom = std::max(1.0F, options.camera_zoom);
        const float visible_span_x = full_span_x / zoom;
        const float visible_span_y = full_span_y / zoom;
        const float half_span_x = visible_span_x * 0.5F;
        const float half_span_y = visible_span_y * 0.5F;
        float center_x = full_center_x + options.camera_pan_x;
        float center_y = full_center_y + options.camera_pan_y;

        if (visible_span_x >= full_span_x) {
            center_x = full_center_x;
        } else {
            center_x = std::clamp(center_x, half_span_x, full_span_x - half_span_x);
        }
        if (visible_span_y >= full_span_y) {
            center_y = full_center_y;
        } else {
            center_y = std::clamp(center_y, half_span_y, full_span_y - half_span_y);
        }

        transform.visible_min_x = std::max(0.0F, center_x - half_span_x);
        transform.visible_max_x = std::min(full_span_x, center_x + half_span_x);
        transform.visible_min_y = std::max(0.0F, center_y - half_span_y);
        transform.visible_max_y = std::min(full_span_y, center_y + half_span_y);
    }

    const float effective_span_x = std::max(1.0F, transform.visible_max_x - transform.visible_min_x);
    const float effective_span_y = std::max(1.0F, transform.visible_max_y - transform.visible_min_y);
    transform.pixels_per_world_x = (transform.screen_bounds.w - 1.0F) / effective_span_x;
    transform.pixels_per_world_y = (transform.screen_bounds.h - 1.0F) / effective_span_y;
    return transform;
}

SDL_FPoint world_to_screen(const ViewportTransform& transform, icss::core::Vec2f position) {
    const float clamped_x = std::clamp(position.x, transform.visible_min_x, transform.visible_max_x);
    const float clamped_y = std::clamp(position.y, transform.visible_min_y, transform.visible_max_y);
    return SDL_FPoint {
        transform.screen_bounds.x + ((clamped_x - transform.visible_min_x) * transform.pixels_per_world_x),
        transform.screen_bounds.y + transform.screen_bounds.h - 1.0F - ((clamped_y - transform.visible_min_y) * transform.pixels_per_world_y),
    };
}

SDL_FPoint world_delta_to_pixels(const ViewportTransform& transform, icss::core::Vec2f delta) {
    return SDL_FPoint {
        delta.x * transform.pixels_per_world_x,
        -delta.y * transform.pixels_per_world_y,
    };
}

void draw_emphasized_line(SDL_Renderer* renderer,
                          SDL_Color color,
                          int x1,
                          int y1,
                          int x2,
                          int y2) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    SDL_RenderDrawLine(renderer, x1 - 1, y1, x2 - 1, y2);
    SDL_RenderDrawLine(renderer, x1 + 1, y1, x2 + 1, y2);
    SDL_RenderDrawLine(renderer, x1, y1 - 1, x2, y2 - 1);
    SDL_RenderDrawLine(renderer, x1, y1 + 1, x2, y2 + 1);
}

RenderContext make_render_context(const ViewerState& state,
                                  const ViewerOptions& options,
                                  const GuiLayout& layout,
                                  TTF_Font* title_font,
                                  TTF_Font* body_font) {
    const auto phase_title = phase_banner_label(state.snapshot.phase);
    const auto phase_note = phase_operator_note(state.snapshot.phase);
    const auto phase_color = phase_accent(state.snapshot.phase);
    const auto authoritative_badge = authoritative_badge_label(state);
    const auto authoritative_color = authoritative_badge_color(state);
    const auto resilience_note = resilience_summary(state);
    const auto recommended_control = recommended_control_label(state);
    const int title_font_h = TTF_FontHeight(title_font);
    const int body_font_h = TTF_FontHeight(body_font);
    constexpr int header_text_gap = 4;
    const int header_text_total_h = title_font_h + header_text_gap + body_font_h;
    const int header_text_y = layout.header_panel.y + std::max(0, (layout.header_panel.h - header_text_total_h) / 2);
    return {
        state,
        options,
        layout,
        title_font,
        body_font,
        phase_title,
        phase_note,
        phase_color,
        authoritative_badge,
        authoritative_color,
        resilience_note,
        recommended_control,
        title_font_h,
        body_font_h,
        header_text_gap,
        header_text_total_h,
        header_text_y,
    };
}

void render_header_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& header_panel = ctx.layout.header_panel;
    const auto& phase_strip = ctx.layout.phase_strip;
    fill_panel(renderer, header_panel, rgba(18, 24, 32), rgba(56, 66, 86));
    SDL_SetRenderDrawColor(renderer, ctx.phase_color.r, ctx.phase_color.g, ctx.phase_color.b, 255);
    SDL_RenderFillRect(renderer, &phase_strip);
    draw_text(renderer, ctx.title_font, phase_strip.x + 14, phase_strip.y + 8, rgba(10, 12, 16), ctx.phase_title);
    const SDL_Rect status_badge {header_panel.x + header_panel.w - 196, header_panel.y + (header_panel.h - 28) / 2, 184, 28};
    fill_panel(renderer, status_badge, rgba(28, 34, 46), ctx.authoritative_color);
    draw_text(renderer,
              ctx.body_font,
              status_badge.x + 10,
              status_badge.y + std::max(0, (status_badge.h - ctx.body_font_h) / 2),
              ctx.authoritative_color,
              ctx.authoritative_badge,
              status_badge.w - 20);
    draw_text(renderer, ctx.title_font, header_panel.x + 360, ctx.header_text_y, rgba(236, 239, 244), "ICSS Tactical Picture");
    draw_text(renderer,
              ctx.body_font,
              header_panel.x + 360,
              ctx.header_text_y + ctx.title_font_h + ctx.header_text_gap,
              rgba(200, 208, 220),
              ctx.phase_note,
              status_badge.x - (header_panel.x + 372));
}

}  // namespace icss::viewer_gui
