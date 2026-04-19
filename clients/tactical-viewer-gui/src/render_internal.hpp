#pragma once

#include <string>

#include "app.hpp"

namespace icss::viewer_gui {

struct RenderContext {
    const ViewerState& state;
    const ViewerOptions& options;
    const GuiLayout& layout;
    TTF_Font* title_font;
    TTF_Font* body_font;
    std::string phase_title;
    std::string phase_note;
    SDL_Color phase_color;
    std::string authoritative_badge;
    SDL_Color authoritative_color;
    std::string resilience_note;
    std::string recommended_control;
    int title_font_h;
    int body_font_h;
    int header_text_gap;
    int header_text_total_h;
    int header_text_y;
};

ViewportTransform make_viewport_transform(const SDL_Rect& map_rect,
                                         int world_width,
                                         int world_height,
                                         const ViewerOptions& options);
SDL_FPoint world_to_screen(const ViewportTransform& transform, icss::core::Vec2f position);
SDL_FPoint world_delta_to_pixels(const ViewportTransform& transform, icss::core::Vec2f delta);
void draw_emphasized_line(SDL_Renderer* renderer,
                          SDL_Color color,
                          int x1,
                          int y1,
                          int x2,
                          int y2);
RenderContext make_render_context(const ViewerState& state,
                                  const ViewerOptions& options,
                                  const GuiLayout& layout,
                                  TTF_Font* title_font,
                                  TTF_Font* body_font);
void render_header_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_map_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_phase_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_decision_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_resilience_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_control_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_setup_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_entity_panel(SDL_Renderer* renderer, const RenderContext& ctx);
void render_event_panel(SDL_Renderer* renderer, const RenderContext& ctx);

}  // namespace icss::viewer_gui
