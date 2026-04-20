#include "app.hpp"

#include <array>
#include <stdexcept>

namespace icss::viewer_gui {
namespace {

bool rects_overlap(const SDL_Rect& lhs, const SDL_Rect& rhs) {
    return lhs.x < rhs.x + rhs.w
        && lhs.x + lhs.w > rhs.x
        && lhs.y < rhs.y + rhs.h
        && lhs.y + lhs.h > rhs.y;
}

std::vector<Button> make_live_control_buttons(const SDL_Rect& control_panel) {
    const int row1_y = control_panel.y + 126;
    const int row2_y = control_panel.y + 164;
    const int button_gap = 10;
    const int button_w = (control_panel.w - 24 - (button_gap * 2)) / 3;
    return {
        {"Start", "Start", {control_panel.x + 12, row1_y, button_w, 30}},
        {"Track", "Track", {control_panel.x + 12 + button_w + button_gap, row1_y, button_w, 30}},
        {"Ready", "Ready", {control_panel.x + 12 + (button_w + button_gap) * 2, row1_y, button_w, 30}},
        {"Engage", "Engage", {control_panel.x + 12, row2_y, button_w, 30}},
        {"Reset", "Reset", {control_panel.x + 12 + button_w + button_gap, row2_y, button_w, 30}},
        {"AAR", "AAR", {control_panel.x + 12 + (button_w + button_gap) * 2, row2_y, button_w, 30}},
    };
}

std::vector<Button> make_setup_buttons(const SDL_Rect& setup_panel) {
    const int row1_y = setup_panel.y + 60;
    const int row2_y = setup_panel.y + 86;
    const int row3_y = setup_panel.y + 112;
    const int row4_y = setup_panel.y + 138;
    const int button_w = 32;
    const int button_h = 18;
    const int button_gap = 4;

    auto row_buttons = [&](int row_y,
                           std::string_view x_dec,
                           std::string_view x_inc,
                           std::string_view y_dec,
                           std::string_view y_inc,
                           std::string_view x_dec_label,
                           std::string_view x_inc_label,
                           std::string_view y_dec_label,
                           std::string_view y_inc_label) {
        const int start_x = setup_panel.x + setup_panel.w - 12 - ((button_w * 4) + (button_gap * 3));
        return std::array<Button, 4>{
            Button{std::string(x_dec), std::string(x_dec_label), {start_x + 0 * (button_w + button_gap), row_y + 4, button_w, button_h}},
            Button{std::string(x_inc), std::string(x_inc_label), {start_x + 1 * (button_w + button_gap), row_y + 4, button_w, button_h}},
            Button{std::string(y_dec), std::string(y_dec_label), {start_x + 2 * (button_w + button_gap), row_y + 4, button_w, button_h}},
            Button{std::string(y_inc), std::string(y_inc_label), {start_x + 3 * (button_w + button_gap), row_y + 4, button_w, button_h}},
        };
    };

    std::vector<Button> buttons;
    const auto target_pos = row_buttons(row1_y, "target_pos_x_dec", "target_pos_x_inc", "target_pos_y_dec", "target_pos_y_inc", "X-", "X+", "Y-", "Y+");
    const auto target_vel = row_buttons(row2_y, "target_vel_x_dec", "target_vel_x_inc", "target_vel_y_dec", "target_vel_y_inc", "X-", "X+", "Y-", "Y+");
    const auto angle_buttons = std::array<Button, 2>{
        Button{"launch_angle_dec", "-A", {setup_panel.x + setup_panel.w - 12 - ((button_w * 2) + button_gap), row3_y + 4, button_w, button_h}},
        Button{"launch_angle_inc", "+A", {setup_panel.x + setup_panel.w - 12 - button_w, row3_y + 4, button_w, button_h}},
    };
    const auto asset_dyn = row_buttons(row4_y, "interceptor_speed_dec", "interceptor_speed_inc", "timeout_dec", "timeout_inc", "S-", "S+", "T-", "T+");
    buttons.insert(buttons.end(), target_pos.begin(), target_pos.end());
    buttons.insert(buttons.end(), target_vel.begin(), target_vel.end());
    buttons.insert(buttons.end(), angle_buttons.begin(), angle_buttons.end());
    buttons.insert(buttons.end(), asset_dyn.begin(), asset_dyn.end());
    return buttons;
}

}  // namespace

GuiLayout build_layout(const ViewerOptions& options) {
    constexpr int margin = 24;
    constexpr int header_x = 24;
    constexpr int header_y = 18;
    constexpr int header_h = 64;
    constexpr int gap_after_header = 36;
    constexpr int gap_after_map = 30;
    constexpr int gap_before_event_panel = 18;
    constexpr int grid_width = 24;
    constexpr int grid_height = 16;
    constexpr int cell_px = 24;
    constexpr int entity_panel_h = 118;
    constexpr int right_panel_gap = 12;
    constexpr int left_panel_w = 330;

    GuiLayout layout;
    layout.header_panel = {header_x, header_y, options.width - (margin * 2), header_h};
    layout.phase_strip = {layout.header_panel.x + 12, layout.header_panel.y + 12, 330, layout.header_panel.h - 24};
    const int content_y = header_y + header_h + gap_after_header;
    layout.map_rect = {margin, content_y, grid_width * cell_px, grid_height * cell_px};
    layout.entity_panel = {margin, layout.map_rect.y + layout.map_rect.h + gap_after_map, layout.map_rect.w, entity_panel_h};
    layout.panel_x = layout.map_rect.x + layout.map_rect.w + margin;
    const int stacked_column_total_h = layout.map_rect.h + gap_after_map + layout.entity_panel.h - right_panel_gap;
    const int panel_h = stacked_column_total_h / 2;
    layout.phase_panel = {layout.panel_x, content_y, left_panel_w, panel_h};
    const int right_panel_x = layout.panel_x + left_panel_w + right_panel_gap;
    layout.decision_panel = {right_panel_x, content_y, options.width - right_panel_x - margin, panel_h};
    layout.resilience_panel = {layout.panel_x, content_y + panel_h + right_panel_gap, left_panel_w, panel_h};
    layout.control_panel = {right_panel_x, content_y + panel_h + right_panel_gap, options.width - right_panel_x - margin, panel_h};
    const int upper_panel_bottom = std::max(layout.entity_panel.y + layout.entity_panel.h,
                                            std::max(layout.resilience_panel.y + layout.resilience_panel.h,
                                                     layout.control_panel.y + layout.control_panel.h));
    const int bottom_panel_y = upper_panel_bottom + gap_before_event_panel;
    const int bottom_panel_h = options.height - bottom_panel_y - margin;
    layout.setup_panel = {right_panel_x, bottom_panel_y, layout.control_panel.w, bottom_panel_h};
    layout.event_panel = {margin,
                          bottom_panel_y,
                          (layout.phase_panel.x + layout.phase_panel.w) - margin,
                          bottom_panel_h};
    layout.buttons = make_live_control_buttons(layout.control_panel);
    const auto setup_buttons = make_setup_buttons(layout.setup_panel);
    layout.buttons.insert(layout.buttons.end(), setup_buttons.begin(), setup_buttons.end());

    const std::array<SDL_Rect, 8> panels {
        layout.header_panel,
        layout.map_rect,
        layout.entity_panel,
        layout.phase_panel,
        layout.decision_panel,
        layout.resilience_panel,
        layout.control_panel,
        layout.setup_panel,
    };
    for (std::size_t i = 0; i < panels.size(); ++i) {
        for (std::size_t j = i + 1; j < panels.size(); ++j) {
            if (rects_overlap(panels[i], panels[j])) {
                throw std::runtime_error("gui layout overlap detected");
            }
        }
    }
    if (rects_overlap(layout.entity_panel, layout.event_panel)
        || rects_overlap(layout.phase_panel, layout.event_panel)
        || rects_overlap(layout.decision_panel, layout.event_panel)
        || rects_overlap(layout.resilience_panel, layout.event_panel)
        || rects_overlap(layout.control_panel, layout.event_panel)
        || rects_overlap(layout.setup_panel, layout.event_panel)) {
        throw std::runtime_error("gui event panel overlaps another panel");
    }
    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
        const auto& rect = layout.buttons[i].rect;
        const auto inside_control = rect.x >= layout.control_panel.x
            && rect.y >= layout.control_panel.y
            && rect.x + rect.w <= layout.control_panel.x + layout.control_panel.w
            && rect.y + rect.h <= layout.control_panel.y + layout.control_panel.h;
        const auto inside_setup = rect.x >= layout.setup_panel.x
            && rect.y >= layout.setup_panel.y
            && rect.x + rect.w <= layout.setup_panel.x + layout.setup_panel.w
            && rect.y + rect.h <= layout.setup_panel.y + layout.setup_panel.h;
        if (!inside_control && !inside_setup) {
            throw std::runtime_error("gui control button exceeds control panel bounds");
        }
        for (std::size_t j = i + 1; j < layout.buttons.size(); ++j) {
            if (rects_overlap(rect, layout.buttons[j].rect)) {
                throw std::runtime_error("gui control buttons overlap");
            }
        }
    }
    return layout;
}

}  // namespace icss::viewer_gui
