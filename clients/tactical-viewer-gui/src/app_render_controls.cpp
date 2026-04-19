#include "render_internal.hpp"

namespace icss::viewer_gui {
namespace {

void draw_button(SDL_Renderer* renderer,
                 const RenderContext& ctx,
                 const Button& button,
                 bool is_live_control) {
    const bool is_review_button = button.action == "Review";
    const bool is_reset_button = button.action == "Reset";
    const bool is_guidance_button = button.action == "Guidance";
    const bool post_command_track_locked = is_guidance_button
        && (ctx.state.snapshot.phase == icss::core::SessionPhase::CommandIssued
            || ctx.state.snapshot.phase == icss::core::SessionPhase::Engaging
            || ctx.state.snapshot.phase == icss::core::SessionPhase::Judged
            || ctx.state.snapshot.phase == icss::core::SessionPhase::Archived);
    const bool enabled = (!is_review_button || review_available(ctx.state)) && !post_command_track_locked;
    const bool recommended = is_live_control && enabled && button.action == ctx.recommended_control;
    const std::string button_label = is_guidance_button
        ? (ctx.state.snapshot.track.active ? "Guidance Off" : "Guidance On")
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
    const int text_x = is_live_control ? button.rect.x + 10 : button.rect.x + 5;
    draw_text(renderer, ctx.body_font, text_x, button.rect.y + (is_live_control ? 5 : 3), text, button_label);
}

}  // namespace

void render_control_panel(SDL_Renderer* renderer, const RenderContext& ctx) {
    const auto& panel = ctx.layout.control_panel;
    fill_panel(renderer, panel, rgba(12, 14, 20), rgba(54, 60, 78));
    draw_text(renderer, ctx.title_font, panel.x + 12, panel.y + 10, rgba(141, 211, 199), "Control Panel");
    const SDL_Rect next_hint_box {panel.x + 12, panel.y + 40, panel.w - 24, 26};
    fill_panel(renderer, next_hint_box, rgba(22, 27, 36), rgba(56, 66, 86));
    draw_text(renderer,
              ctx.body_font,
              next_hint_box.x + 10,
              next_hint_box.y + 5,
              rgba(148, 156, 172),
              ctx.recommended_control.empty() ? "Next recommended control: none" : "Next recommended control: " + ctx.recommended_control,
              next_hint_box.w - 20);

    draw_text(renderer,
              ctx.body_font,
              panel.x + 12,
              panel.y + 79,
              rgba(148, 156, 172),
              "Guidance");

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
    draw_text(renderer, ctx.body_font, panel.x + 12, panel.y + 38, rgba(148, 156, 172), "Target jitter only; origin fixed at (0,0)");
    const SDL_Rect target_pos_box {panel.x + 12, panel.y + 60, panel.w - 24, 28};
    const SDL_Rect target_vel_box {panel.x + 12, panel.y + 88, panel.w - 24, 28};
    const SDL_Rect launch_box {panel.x + 12, panel.y + 116, panel.w - 24, 28};
    const SDL_Rect asset_dyn_box {panel.x + 12, panel.y + 144, panel.w - 24, 28};
    for (const auto& box : {target_pos_box, target_vel_box, launch_box, asset_dyn_box}) {
        fill_panel(renderer, box, rgba(22, 27, 36), rgba(56, 66, 86));
    }
    draw_text(renderer,
              ctx.body_font,
              target_pos_box.x + 8,
              target_pos_box.y + 5,
              rgba(188, 198, 214),
              "Target  " + std::to_string(ctx.state.planned_scenario.target_start_x) + "," + std::to_string(ctx.state.planned_scenario.target_start_y));
    draw_text(renderer,
              ctx.body_font,
              target_vel_box.x + 8,
              target_vel_box.y + 5,
              rgba(188, 198, 214),
              "Velocity  " + std::to_string(ctx.state.planned_scenario.target_velocity_x) + "," + std::to_string(ctx.state.planned_scenario.target_velocity_y));
    draw_text(renderer,
              ctx.body_font,
              launch_box.x + 8,
              launch_box.y + 5,
              rgba(188, 198, 214),
              "Origin  0,0 | Launch  " + std::to_string(ctx.state.planned_scenario.launch_angle_deg) + " deg");
    draw_text(renderer,
              ctx.body_font,
              asset_dyn_box.x + 8,
              asset_dyn_box.y + 5,
              rgba(188, 198, 214),
              "Speed  " + std::to_string(ctx.state.planned_scenario.interceptor_speed_per_tick)
                  + " | Timeout  " + std::to_string(ctx.state.planned_scenario.engagement_timeout_ticks));
    for (const auto& button : ctx.layout.buttons) {
        if (!is_live_control_action(button.action)) {
            draw_button(renderer, ctx, button, false);
        }
    }
}

}  // namespace icss::viewer_gui
