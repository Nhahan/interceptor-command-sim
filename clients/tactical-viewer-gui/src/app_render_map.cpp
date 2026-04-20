#include "render_internal.hpp"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace icss::viewer_gui {
namespace {

constexpr float kTrailDashPx = 8.0F;
constexpr float kTrailGapPx = 6.0F;
constexpr float kVelocityProjectionTicks = 6.0F;

int major_grid_step(float visible_span) {
    constexpr int kCandidates[] {24, 48, 96, 192};
    for (const auto candidate : kCandidates) {
        if ((visible_span / static_cast<float>(candidate)) <= 8.0F) {
            return candidate;
        }
    }
    return 192;
}

void draw_dashed_segment(SDL_Renderer* renderer,
                         SDL_Color color,
                         SDL_FPoint start,
                         SDL_FPoint end,
                         float dash_length,
                         float gap_length) {
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 0.5F) {
        return;
    }
    const float ux = dx / length;
    const float uy = dy / length;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    float traveled = 0.0F;
    while (traveled < length) {
        const float dash_end = std::min(length, traveled + dash_length);
        const SDL_FPoint dash_start_point {start.x + (ux * traveled), start.y + (uy * traveled)};
        const SDL_FPoint dash_end_point {start.x + (ux * dash_end), start.y + (uy * dash_end)};
        SDL_RenderDrawLineF(renderer, dash_start_point.x, dash_start_point.y, dash_end_point.x, dash_end_point.y);
        traveled = dash_end + gap_length;
    }
}

void draw_history(SDL_Renderer* renderer,
                  const ViewportTransform& transform,
                  const std::deque<icss::core::Vec2f>& history,
                  SDL_Color color) {
    if (history.size() < 2) {
        return;
    }
    SDL_FPoint previous = world_to_screen(transform, history.front());
    for (std::size_t index = 1; index < history.size(); ++index) {
        const auto current = world_to_screen(transform, history[index]);
        draw_dashed_segment(renderer, color, previous, current, kTrailDashPx, kTrailGapPx);
        previous = current;
    }
}

void draw_filled_diamond(SDL_Renderer* renderer,
                         int center_x,
                         int center_y,
                         int radius,
                         SDL_Color fill,
                         SDL_Color border,
                         bool filled) {
    if (filled) {
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        for (int dy = -radius; dy <= radius; ++dy) {
            const int half_width = radius - std::abs(dy);
            SDL_RenderDrawLine(renderer,
                               center_x - half_width,
                               center_y + dy,
                               center_x + half_width,
                               center_y + dy);
        }
    }
    SDL_Point outline[5] {
        {center_x, center_y - radius},
        {center_x + radius, center_y},
        {center_x, center_y + radius},
        {center_x - radius, center_y},
        {center_x, center_y - radius},
    };
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawLines(renderer, outline, 5);
}

void draw_target_square(SDL_Renderer* renderer,
                        int center_x,
                        int center_y,
                        int radius,
                        SDL_Color fill,
                        SDL_Color border,
                        bool filled) {
    SDL_Rect rect {center_x - radius, center_y - radius, radius * 2, radius * 2};
    if (filled) {
        SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
        SDL_RenderFillRect(renderer, &rect);
    }
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void draw_entity(SDL_Renderer* renderer,
                 const RenderContext& ctx,
                 const ViewportTransform& transform,
                 const icss::core::EntityState& entity,
                 SDL_Color color) {
    const auto center = (&entity == &ctx.state.snapshot.target)
        ? world_to_screen(transform, ctx.state.snapshot.target_world_position)
        : world_to_screen(transform, ctx.state.snapshot.interceptor_world_position);
    const bool filled = entity.active;
    const SDL_Color fill = filled ? color : rgba(color.r / 3, color.g / 3, color.b / 3);
    const SDL_Color border = filled ? rgba(240, 244, 255) : rgba(color.r / 2, color.g / 2, color.b / 2);
    if (&entity == &ctx.state.snapshot.target) {
        draw_target_square(renderer,
                           static_cast<int>(center.x),
                           static_cast<int>(center.y),
                           3,
                           fill,
                           border,
                           filled);
    } else {
        draw_filled_diamond(renderer,
                            static_cast<int>(center.x),
                            static_cast<int>(center.y),
                            4,
                            fill,
                            border,
                            filled);
    }
}

void draw_arrow(SDL_Renderer* renderer,
                SDL_Color color,
                SDL_FPoint start,
                SDL_FPoint end) {
    draw_emphasized_line(renderer,
                         color,
                         static_cast<int>(start.x),
                         static_cast<int>(start.y),
                         static_cast<int>(end.x),
                         static_cast<int>(end.y));
    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float length = std::sqrt((dx * dx) + (dy * dy));
    if (length <= 1.0F) {
        return;
    }
    const float ux = dx / length;
    const float uy = dy / length;
    const float px = -uy;
    const float py = ux;
    constexpr float kArrowBack = 10.0F;
    constexpr float kArrowWidth = 5.0F;
    const SDL_FPoint left {
        end.x - (ux * kArrowBack) + (px * kArrowWidth),
        end.y - (uy * kArrowBack) + (py * kArrowWidth),
    };
    const SDL_FPoint right {
        end.x - (ux * kArrowBack) - (px * kArrowWidth),
        end.y - (uy * kArrowBack) - (py * kArrowWidth),
    };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(renderer, end.x, end.y, left.x, left.y);
    SDL_RenderDrawLineF(renderer, end.x, end.y, right.x, right.y);
}

void draw_corner_brackets(SDL_Renderer* renderer,
                          SDL_FPoint center,
                          float half_extent,
                          float arm_length,
                          SDL_Color color) {
    const float left = center.x - half_extent;
    const float right = center.x + half_extent;
    const float top = center.y - half_extent;
    const float bottom = center.y + half_extent;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLineF(renderer, left, top, left + arm_length, top);
    SDL_RenderDrawLineF(renderer, left, top, left, top + arm_length);
    SDL_RenderDrawLineF(renderer, right, top, right - arm_length, top);
    SDL_RenderDrawLineF(renderer, right, top, right, top + arm_length);
    SDL_RenderDrawLineF(renderer, left, bottom, left + arm_length, bottom);
    SDL_RenderDrawLineF(renderer, left, bottom, left, bottom - arm_length);
    SDL_RenderDrawLineF(renderer, right, bottom, right - arm_length, bottom);
    SDL_RenderDrawLineF(renderer, right, bottom, right, bottom - arm_length);
}

void draw_predicted_marker(SDL_Renderer* renderer, SDL_FPoint center) {
    const int cx = static_cast<int>(std::lround(center.x));
    const int cy = static_cast<int>(std::lround(center.y));
    SDL_Rect halo {cx - 9, cy - 9, 18, 18};
    SDL_SetRenderDrawColor(renderer, 12, 18, 12, 190);
    SDL_RenderFillRect(renderer, &halo);
    draw_emphasized_line(renderer,
                         rgba(182, 255, 182, 240),
                         cx - 8,
                         cy - 8,
                         cx + 8,
                         cy + 8);
    draw_emphasized_line(renderer,
                         rgba(182, 255, 182, 240),
                         cx - 8,
                         cy + 8,
                         cx + 8,
                         cy - 8);
    draw_filled_diamond(renderer,
                        cx,
                        cy,
                        7,
                        rgba(0, 0, 0, 0),
                        rgba(182, 255, 182, 220),
                        false);
}

void draw_launch_origin_marker(SDL_Renderer* renderer,
                               const RenderContext& ctx,
                               const ViewportTransform& transform,
                               const SDL_Rect& map_rect) {
    const auto origin = world_to_screen(transform, {0.0F, 0.0F});
    const int cx = static_cast<int>(std::lround(origin.x));
    const int cy = static_cast<int>(std::lround(origin.y));
    SDL_SetRenderDrawColor(renderer, 120, 190, 255, 200);
    SDL_RenderDrawLine(renderer, cx - 8, cy, cx + 8, cy);
    SDL_RenderDrawLine(renderer, cx, cy - 8, cx, cy + 8);
    SDL_Rect halo {cx - 4, cy - 4, 8, 8};
    SDL_RenderDrawRect(renderer, &halo);
    const int label_x = std::min(map_rect.x + map_rect.w - 70, cx + 10);
    const int label_y = std::max(map_rect.y + 8, cy - 18);
    draw_text(renderer, ctx.body_font, label_x, label_y, rgba(120, 190, 255), "ORIGIN");
}

}  // namespace

void render_map_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& map_rect = ctx.layout.map_rect;
    const auto transform = make_viewport_transform(map_rect, ctx.state, ctx.options);
    const auto grid_step_x = major_grid_step(transform.visible_max_x - transform.visible_min_x);
    const auto grid_step_y = major_grid_step(transform.visible_max_y - transform.visible_min_y);
    fill_panel(renderer, map_rect, rgba(36, 41, 52), rgba(74, 80, 96));
    SDL_SetRenderDrawColor(renderer, 74, 80, 96, 110);
    const int x_start = static_cast<int>(std::ceil(transform.visible_min_x / static_cast<float>(grid_step_x))) * grid_step_x;
    const int x_end = static_cast<int>(std::floor(transform.visible_max_x));
    for (int x = x_start; x <= x_end; x += grid_step_x) {
        const int px = static_cast<int>(world_to_screen(transform, {static_cast<float>(x), 0.0F}).x);
        SDL_RenderDrawLine(renderer, px, map_rect.y, px, map_rect.y + map_rect.h);
    }
    const int y_start = static_cast<int>(std::ceil(transform.visible_min_y / static_cast<float>(grid_step_y))) * grid_step_y;
    const int y_end = static_cast<int>(std::floor(transform.visible_max_y));
    for (int y = y_start; y <= y_end; y += grid_step_y) {
        const int py = static_cast<int>(world_to_screen(transform, {0.0F, static_cast<float>(y)}).y);
        SDL_RenderDrawLine(renderer, map_rect.x, py, map_rect.x + map_rect.w, py);
    }
    for (int x = x_start; x <= x_end; x += grid_step_x) {
        const int px = static_cast<int>(world_to_screen(transform, {static_cast<float>(x), 0.0F}).x);
        draw_text(renderer, ctx.body_font, px + 2, map_rect.y + map_rect.h + 6, rgba(140, 149, 168), std::to_string(x));
    }
    for (int y = y_start; y <= y_end; y += grid_step_y) {
        const int py = static_cast<int>(world_to_screen(transform, {0.0F, static_cast<float>(y)}).y);
        draw_text(renderer, ctx.body_font, map_rect.x - 24, py + 2, rgba(140, 149, 168), std::to_string(y));
    }

    draw_launch_origin_marker(renderer, ctx, transform, map_rect);
    draw_history(renderer, transform, ctx.state.target_history, rgba(244, 67, 54, 110));
    draw_history(renderer, transform, ctx.state.interceptor_history, rgba(66, 165, 245, 110));
    const auto target_center = world_to_screen(transform, ctx.state.snapshot.target_world_position);
    const auto interceptor_center = world_to_screen(transform, ctx.state.snapshot.interceptor_world_position);
    const bool target_motion_visible = target_motion_visual_visible(ctx.state);
    const bool interceptor_motion_visible = interceptor_motion_visual_visible(ctx.state);
    const bool engagement_live = engagement_visual_visible(ctx.state);

    if (target_motion_visible) {
        const auto target_vector_px = world_delta_to_pixels(
            transform,
            {ctx.state.snapshot.target_velocity.x * kVelocityProjectionTicks,
             ctx.state.snapshot.target_velocity.y * kVelocityProjectionTicks});
        const SDL_FPoint target_vector_end {target_center.x + target_vector_px.x, target_center.y + target_vector_px.y};
        draw_arrow(renderer, rgba(244, 67, 54, 220), target_center, target_vector_end);
        draw_corner_brackets(renderer, target_center, 13.0F, 7.0F, rgba(255, 214, 102, 220));
        draw_text(renderer,
                  ctx.body_font,
                  static_cast<int>(std::lround(target_center.x)) + 18,
                  static_cast<int>(std::lround(target_center.y)) - 26,
                  rgba(255, 214, 102),
                  "TRACK FILE");
        draw_text(renderer,
                  ctx.body_font,
                  static_cast<int>(std::lround(target_center.x)) + 18,
                  static_cast<int>(std::lround(target_center.y)) - 10,
                  rgba(188, 198, 214),
                  "res="
                      + std::string(ctx.state.snapshot.track.measurement_valid
                                        ? format_fixed_1(ctx.state.snapshot.track.measurement_residual_distance)
                                        : "n/a")
                      + " cov=" + format_fixed_1(ctx.state.snapshot.track.covariance_trace));
        draw_text(renderer,
                  ctx.body_font,
                  static_cast<int>(std::lround(target_center.x)) + 18,
                  static_cast<int>(std::lround(target_center.y)) + 6,
                  rgba(188, 198, 214),
                  "age=" + std::to_string(ctx.state.snapshot.track.measurement_age_ticks)
                      + " miss=" + std::to_string(ctx.state.snapshot.track.missed_updates));
    }
    if (interceptor_motion_visible) {
        const auto interceptor_vector_px = world_delta_to_pixels(
            transform,
            {ctx.state.snapshot.interceptor_velocity.x * kVelocityProjectionTicks,
             ctx.state.snapshot.interceptor_velocity.y * kVelocityProjectionTicks});
        const SDL_FPoint interceptor_vector_end {interceptor_center.x + interceptor_vector_px.x, interceptor_center.y + interceptor_vector_px.y};
        draw_arrow(renderer, rgba(66, 165, 245, 220), interceptor_center, interceptor_vector_end);
    }
    if (engagement_live) {
        if (predicted_marker_visual_visible(ctx.state)) {
            const auto predicted_center = world_to_screen(transform, ctx.state.snapshot.predicted_intercept_position);
            draw_predicted_marker(renderer, predicted_center);
        }
        draw_emphasized_line(renderer,
                             rgba(255, 149, 0, 255),
                             static_cast<int>(interceptor_center.x),
                             static_cast<int>(interceptor_center.y),
                             static_cast<int>(target_center.x),
                             static_cast<int>(target_center.y));
        draw_corner_brackets(renderer, target_center, 11.0F, 6.0F, rgba(255, 149, 0, 235));
    }

    draw_entity(renderer, ctx, transform, ctx.state.snapshot.target, rgba(244, 67, 54));
    draw_entity(renderer, ctx, transform, ctx.state.snapshot.interceptor, rgba(66, 165, 245));
}

}  // namespace icss::viewer_gui
