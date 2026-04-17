#include <cassert>
#include <filesystem>

#include "icss/core/runtime.hpp"

int main() {
    namespace fs = std::filesystem;
    using namespace icss::core;

    const RuntimeConfig config = default_runtime_config(fs::path{ICSS_REPO_ROOT});
    assert(config.server.bind_host == "127.0.0.1");
    assert(config.server.tick_rate_hz == 20);
    assert(config.server.tcp_port == 4000);
    assert(config.server.udp_port == 4001);
    assert(config.server.heartbeat_timeout_ms == 3000);
    assert(config.scenario.name == "basic_intercept_training");
    assert(config.scenario.telemetry_interval_ms == 200);
    assert(config.scenario.world_width == 576);
    assert(config.scenario.world_height == 384);
    assert(config.scenario.target_velocity_x == 5);
    assert(config.scenario.target_velocity_y == -3);
    assert(config.scenario.interceptor_speed_per_tick == 14);
    assert(config.scenario.intercept_radius == 12);
    assert(config.scenario.engagement_timeout_ticks == 26);
    assert(config.logging.level == "info");
    assert(config.logging.aar_enabled);
    assert(config.logging.aar_output_dir == fs::path{"assets/sample-aar"});
    return 0;
}
