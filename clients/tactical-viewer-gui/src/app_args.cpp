#include "app.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <vector>

namespace icss::viewer_gui {
namespace {

std::filesystem::path resolve_repo_root(std::filesystem::path path) {
    if (std::filesystem::exists(path / "configs/server.example.yaml")) {
        return path;
    }
    if (std::filesystem::exists(path.parent_path() / "configs/server.example.yaml")) {
        return path.parent_path();
    }
    return path;
}

std::filesystem::path default_font_path() {
    const std::vector<std::filesystem::path> candidates {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("no usable monospace font found; pass --font PATH");
}

std::vector<std::string> split_controls(const std::string& value) {
    std::vector<std::string> items;
    std::string current;
    for (char ch : value) {
        if (ch == ',') {
            if (!current.empty()) {
                items.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        items.push_back(current);
    }
    return items;
}

}  // namespace

ViewerOptions parse_args(int argc, char** argv) {
    ViewerOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        auto require_value = [&](std::string_view label) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error("missing value for " + std::string(label));
            }
            return argv[++i];
        };
        if (arg == "--host") {
            options.host = require_value(arg);
            continue;
        }
        if (arg == "--udp-port") {
            options.udp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--tcp-port") {
            options.tcp_port = static_cast<std::uint16_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--session-id") {
            options.session_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--sender-id") {
            options.sender_id = static_cast<std::uint32_t>(std::stoul(require_value(arg)));
            continue;
        }
        if (arg == "--duration-ms") {
            options.duration_ms = std::stoull(require_value(arg));
            continue;
        }
        if (arg == "--heartbeat-interval-ms") {
            options.heartbeat_interval_ms = std::stoull(require_value(arg));
            continue;
        }
        if (arg == "--repo-root") {
            options.repo_root = require_value(arg);
            continue;
        }
        if (arg == "--camera-zoom") {
            options.camera_zoom = std::stof(require_value(arg));
            continue;
        }
        if (arg == "--camera-pan-x") {
            options.camera_pan_x = std::stof(require_value(arg));
            continue;
        }
        if (arg == "--camera-pan-y") {
            options.camera_pan_y = std::stof(require_value(arg));
            continue;
        }
        if (arg == "--tcp-frame-format") {
            options.tcp_frame_format = require_value(arg);
            continue;
        }
        if (arg == "--auto-controls") {
            options.auto_controls = split_controls(require_value(arg));
            options.auto_control_script = true;
            continue;
        }
        if (arg == "--width") {
            options.width = std::stoi(require_value(arg));
            continue;
        }
        if (arg == "--height") {
            options.height = std::stoi(require_value(arg));
            continue;
        }
        if (arg == "--dump-state") {
            options.dump_state_path = require_value(arg);
            continue;
        }
        if (arg == "--dump-frame") {
            options.dump_frame_path = require_value(arg);
            continue;
        }
        if (arg == "--dump-golden-state") {
            options.dump_golden_state_path = require_value(arg);
            continue;
        }
        if (arg == "--font") {
            options.font_path = require_value(arg);
            continue;
        }
        if (arg == "--hidden") {
            options.hidden = true;
            continue;
        }
        if (arg == "--headless") {
            options.hidden = true;
            options.headless = true;
            continue;
        }
        if (arg == "--auto-control-script") {
            options.auto_control_script = true;
            continue;
        }
        if (arg == "--help") {
            std::printf(
                "usage: icss_tactical_display_gui [--host HOST] [--udp-port PORT] [--tcp-port PORT]\n"
                "       [--session-id ID] [--sender-id ID] [--duration-ms MS] [--heartbeat-interval-ms MS] [--repo-root PATH]\n"
                "       [--tcp-frame-format json|binary] [--camera-zoom X] [--camera-pan-x W] [--camera-pan-y W] [--width PX] [--height PX]\n"
                "       [--dump-state PATH] [--dump-frame PATH] [--dump-golden-state PATH] [--font PATH] [--hidden] [--headless]\n"
                "       [--auto-control-script] [--auto-controls Start,Track,...]\n");
            std::exit(0);
        }
        throw std::runtime_error("unknown argument: " + std::string(arg));
    }
    if (options.font_path.empty()) {
        options.font_path = default_font_path();
    }
    options.camera_zoom = std::max(1.0F, options.camera_zoom);
    return options;
}

icss::core::ScenarioConfig default_viewer_scenario(const std::filesystem::path& repo_root) {
    try {
        return icss::core::load_runtime_config(resolve_repo_root(repo_root)).scenario;
    } catch (const std::exception&) {
        return icss::core::ScenarioConfig {};
    }
}

}  // namespace icss::viewer_gui
