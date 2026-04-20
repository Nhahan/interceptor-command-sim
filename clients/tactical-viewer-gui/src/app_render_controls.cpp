#include "render_internal.hpp"

#include <algorithm>

namespace icss::viewer_gui {
namespace {

void draw_button(SDL_Renderer* renderer,
                 const RenderContext& ctx,
                 const Button& button,
                 bool is_live_control) {
    const bool is_reset_button = button.action == "Reset";
    const bool enabled = is_live_control
        ? control_button_enabled(button.action, ctx.state)
        : true;
    const bool recommended = is_live_control && enabled && button.action == ctx.recommended_control;
    const std::string button_label = is_live_control
        ? control_display_label(button.action, ctx.state)
        : button.label;
    const auto fill = is_reset_button
        ? (enabled ? rgba(92, 42, 52) : rgba(56, 36, 40))
        : (is_live_control
            ? (recommended ? ctx.phase_color : (enabled ? rgba(28, 55, 86) : rgba(38, 38, 38)))
            : rgba(38, 44, 58));
    const auto border = is_reset_button
        ? (enabled ? rgba(224, 108, 146) : rgba(96, 72, 80))
        : (is_live_control
            ? (recommended ? rgba(240, 244, 255) : (enabled ? rgba(91, 126, 168) : rgba(72, 72, 72)))
            : rgba(88, 102, 126));
    const auto text = is_reset_button
        ? (enabled ? rgba(255, 220, 230) : rgba(148, 156, 172))
        : (is_live_control
            ? (recommended ? rgba(10, 12, 16) : (enabled ? rgba(238, 245, 255) : rgba(148, 156, 172)))
            : rgba(188, 198, 214));
    if (is_reset_button && enabled) {
        const SDL_Rect glow_outer {button.rect.x - 1, button.rect.y - 1, button.rect.w + 2, button.rect.h + 2};
        SDL_SetRenderDrawColor(renderer, 255, 128, 168, 96);
        SDL_RenderDrawRect(renderer, &glow_outer);
    }
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &button.rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &button.rect);
    if (is_reset_button) {
        const SDL_Rect inner {button.rect.x + 1, button.rect.y + 1, button.rect.w - 2, button.rect.h - 2};
        SDL_SetRenderDrawColor(renderer, 255, 168, 196, enabled ? 168 : 84);
        SDL_RenderDrawRect(renderer, &inner);
    }
    if (recommended) {
        const SDL_Rect glow {button.rect.x - 2, button.rect.y - 2, button.rect.w + 4, button.rect.h + 4};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
        SDL_RenderDrawRect(renderer, &glow);
    }
    int text_w = 0;
    int text_h = 0;
    TTF_SizeUTF8(ctx.body_font, button_label.c_str(), &text_w, &text_h);
    const int text_x = button.rect.x + std::max(6, (button.rect.w - text_w) / 2);
    const int text_y = button.rect.y + std::max(3, (button.rect.h - text_h) / 2);
    draw_text(renderer, ctx.body_font, text_x, text_y, text, button_label);
}

}  // namespace

void render_control_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.control_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(141, 211, 199), "Fire Control");
    const SDL_Rect next_hint_box {panel.x + 12, panel.y + 40, panel.w - 24, 24};
    fill_panel(renderer, next_hint_box, rgba(22, 27, 36), rgba(56, 66, 86));
    const auto recommended_label = ctx.recommended_control.empty()
        ? std::string("wait")
        : control_display_label(ctx.recommended_control, ctx.state);
    draw_text(renderer,
              ctx.body_font,
              next_hint_box.x + 10,
              next_hint_box.y + 4,
              rgba(148, 156, 172),
              "Next Control: " + recommended_label,
              next_hint_box.w - 20);
    const SDL_Rect status_box {panel.x + 12, panel.y + 70, panel.w - 24, 42};
    fill_panel(renderer, status_box, rgba(18, 23, 32), rgba(56, 66, 86));
    draw_text(renderer,
              ctx.body_font,
              status_box.x + 10,
              status_box.y + 6,
              rgba(188, 198, 214),
              "Mode  " + std::string(ctx.state.effective_track_active ? "tracked" : "unguided")
                  + "  |  Track  " + std::string(ctx.state.snapshot.track.active ? "live" : "dropped"),
              status_box.w - 20);
    draw_text(renderer,
              ctx.body_font,
              status_box.x + 10,
              status_box.y + 20,
              rgba(148, 156, 172),
              control_panel_hint(ctx.state),
              status_box.w - 20);

    for (const auto& button : ctx.layout.buttons) {
        if (is_live_control_action(button.action)) {
            draw_button(renderer, ctx, button, true);
        }
    }
}

void render_setup_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.setup_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(141, 211, 199), "Scenario Setup");
    draw_text(renderer,
              ctx.body_font,
              panel.x + 12,
              panel.y + 38,
              rgba(148, 156, 172),
              "Applies on next Start. Origin stays fixed at (0,0).",
              panel.w - 24);
    const SDL_Rect target_pos_box {panel.x + 12, panel.y + 60, panel.w - 24, 24};
    const SDL_Rect target_vel_box {panel.x + 12, panel.y + 86, panel.w - 24, 24};
    const SDL_Rect launch_box {panel.x + 12, panel.y + 112, panel.w - 24, 24};
    const SDL_Rect asset_dyn_box {panel.x + 12, panel.y + 138, panel.w - 24, 24};
    for (const auto& box : {target_pos_box, target_vel_box, launch_box, asset_dyn_box}) {
        fill_panel(renderer, box, rgba(22, 27, 36), rgba(56, 66, 86));
    }
    const int label_x = panel.x + 18;
    const int value_x = panel.x + 112;
    const int button_gutter_x = panel.x + panel.w - 12 - ((32 * 4) + (4 * 3));
    SDL_SetRenderDrawColor(renderer, 56, 66, 86, 255);
    SDL_RenderDrawLine(renderer, button_gutter_x - 8, target_pos_box.y + 2, button_gutter_x - 8, asset_dyn_box.y + asset_dyn_box.h - 2);
    auto draw_setup_row = [&](const SDL_Rect& box,
                              std::string_view label,
                              const std::string& value,
                              SDL_Color value_color = rgba(188, 198, 214)) {
        draw_text(renderer, ctx.body_font, label_x, box.y + 6, rgba(148, 156, 172), std::string(label));
        draw_text(renderer, ctx.body_font, value_x, box.y + 6, value_color, value, (button_gutter_x - 16) - value_x);
    };
    draw_setup_row(target_pos_box,
                   "Target",
                   std::to_string(ctx.state.planned_scenario.target_start_x)
                       + ", " + std::to_string(ctx.state.planned_scenario.target_start_y));
    draw_setup_row(target_vel_box,
                   "Velocity",
                   std::to_string(ctx.state.planned_scenario.target_velocity_x)
                       + ", " + std::to_string(ctx.state.planned_scenario.target_velocity_y));
    draw_setup_row(launch_box,
                   "Launch",
                   std::to_string(ctx.state.planned_scenario.launch_angle_deg) + " deg");
    draw_setup_row(asset_dyn_box,
                   "Speed/TO",
                   std::to_string(ctx.state.planned_scenario.interceptor_speed_per_tick)
                       + " / " + std::to_string(ctx.state.planned_scenario.engagement_timeout_ticks));
    for (const auto& button : ctx.layout.buttons) {
        if (!is_live_control_action(button.action)) {
            draw_button(renderer, ctx, button, false);
        }
    }
}

}  // namespace icss::viewer_gui
