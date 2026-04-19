#include "app.hpp"

#include <chrono>
#include <cstdio>
#include <stdexcept>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::viewer_gui {
namespace {

void initialize_preview_state(ViewerState& state, const ViewerOptions& options) {
    state.planned_scenario = default_viewer_scenario(options.repo_root);
    state.snapshot.target.id = "target-alpha";
    state.snapshot.asset.id = "asset-interceptor";
    sync_preview_from_planned_scenario(state);
    state.snapshot.viewer_connection = icss::core::ConnectionState::Disconnected;
}

}  // namespace
}  // namespace icss::viewer_gui

int main(int argc, char** argv) {
    using namespace icss::viewer_gui;
    try {
        const auto options = parse_args(argc, argv);
        if (options.headless) {
            SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        }
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        }
        if (TTF_Init() != 0) {
            throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
        }

        Uint32 window_flags = options.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN;
        SDL_Window* window = SDL_CreateWindow("ICSS Tactical Viewer",
                                              SDL_WINDOWPOS_CENTERED,
                                              SDL_WINDOWPOS_CENTERED,
                                              options.width,
                                              options.height,
                                              window_flags);
        if (!window) {
            throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
        }
        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());
        }

        TTF_Font* title_font = TTF_OpenFont(options.font_path.string().c_str(), 20);
        TTF_Font* body_font = TTF_OpenFont(options.font_path.string().c_str(), 15);
        if (!title_font || !body_font) {
            throw std::runtime_error(std::string("failed to open font: ") + TTF_GetError());
        }

        ViewerState state;
        initialize_preview_state(state, options);

#if !defined(_WIN32)
        UdpSocket socket(options.host);
        TcpSocket control_socket;
        const auto frame_mode = parse_frame_mode(options.tcp_frame_format);
        std::uint64_t sequence = 1;
        send_viewer_join(socket, options, sequence);
        state.control.last_message = "viewer registered";
        state.last_join_attempt_ms = SDL_GetTicks64();
#endif

        bool running = true;
        const auto start_ticks = SDL_GetTicks64();
        auto next_heartbeat = start_ticks + options.heartbeat_interval_ms;
        while (running) {
            SDL_Event event {};
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN && !options.headless) {
                    const int x = event.button.x;
                    const int y = event.button.y;
                    for (const auto& button : build_layout(options).buttons) {
                        if (x >= button.rect.x && x < button.rect.x + button.rect.w
                            && y >= button.rect.y && y < button.rect.y + button.rect.h) {
#if !defined(_WIN32)
                            perform_control_action(button.action, state, options, frame_mode, control_socket);
#endif
                            break;
                        }
                    }
                }
                if (event.type == SDL_MOUSEWHEEL && !options.headless) {
                    int mouse_x = 0;
                    int mouse_y = 0;
                    SDL_GetMouseState(&mouse_x, &mouse_y);
                    const auto layout = build_layout(options);
                    const auto& panel = layout.event_panel;
                    if (mouse_x >= panel.x && mouse_x < panel.x + panel.w
                        && mouse_y >= panel.y && mouse_y < panel.y + panel.h) {
                        if (event.wheel.y > 0) {
                            const auto delta = static_cast<std::size_t>(event.wheel.y * 3);
                            state.timeline_scroll_lines = delta > state.timeline_scroll_lines
                                ? 0
                                : state.timeline_scroll_lines - delta;
                        } else if (event.wheel.y < 0) {
                            state.timeline_scroll_lines += static_cast<std::size_t>((-event.wheel.y) * 3);
                        }
                    }
                }
                if (event.type == SDL_KEYDOWN && !options.headless) {
                    switch (event.key.keysym.sym) {
                    case SDLK_HOME:
                        state.timeline_scroll_lines = 0;
                        break;
                    case SDLK_END:
                        state.timeline_scroll_lines = 1024;
                        break;
                    case SDLK_PAGEUP:
                        state.timeline_scroll_lines = state.timeline_scroll_lines > 8
                            ? state.timeline_scroll_lines - 8
                            : 0;
                        break;
                    case SDLK_PAGEDOWN:
                        state.timeline_scroll_lines += 8;
                        break;
                    default:
                        break;
                    }
                }
            }
            const auto now = SDL_GetTicks64();
            if (options.duration_ms > 0 && now - start_ticks >= options.duration_ms) {
                running = false;
            }

#if !defined(_WIN32)
            if (now >= next_heartbeat) {
                send_viewer_heartbeat(socket, options, sequence, state);
                next_heartbeat = now + options.heartbeat_interval_ms;
            }
            receive_datagrams(socket, options, state, now);
            if ((state.received_snapshot || state.received_telemetry)
                && state.last_datagram_received_ms > 0
                && now > state.last_datagram_received_ms + (options.heartbeat_interval_ms * 2)
                && now > state.last_join_attempt_ms + options.heartbeat_interval_ms) {
                send_viewer_join(socket, options, sequence);
                state.last_join_attempt_ms = now;
                push_timeline_entry(state, "[viewer] rejoin requested after stale telemetry");
            }
            if (options.auto_control_script) {
                static const std::vector<std::string> kDefaultScript {
                    "Start", "Guidance", "Activate", "Command", "Review", "Reset", "Start"
                };
                const auto& script = options.auto_controls.empty() ? kDefaultScript : options.auto_controls;
                if (state.control.auto_step < script.size() && now >= state.control.auto_last_action_ms + 140) {
                    const auto& next_action = script[state.control.auto_step];
                    const bool requires_judgment = (next_action == "Review")
                        && state.snapshot.command_status != icss::core::CommandLifecycle::None
                        && !state.snapshot.judgment.ready;
                    if (!requires_judgment) {
                        perform_control_action(next_action, state, options, frame_mode, control_socket);
                        state.control.auto_last_action_ms = now;
                        ++state.control.auto_step;
                    }
                }
            }
#endif
            render_gui(renderer, title_font, body_font, state, options);
            SDL_Delay(16);
        }

        write_dump_state(options.dump_state_path, state, options);
        write_dump_golden_state(options.dump_golden_state_path, state);
        write_dump_frame(renderer, options.dump_frame_path);
        std::printf("received_snapshot=%s\n", state.received_snapshot ? "true" : "false");
        std::printf("received_telemetry=%s\n", state.received_telemetry ? "true" : "false");
        std::printf("snapshot_sequence=%llu\n", static_cast<unsigned long long>(state.snapshot.header.snapshot_sequence));
        std::printf("connection_state=%s\n", icss::core::to_string(state.snapshot.viewer_connection));
        std::printf("freshness=%s\n", icss::view::freshness_label(state.snapshot).c_str());

        TTF_CloseFont(title_font);
        TTF_CloseFont(body_font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
