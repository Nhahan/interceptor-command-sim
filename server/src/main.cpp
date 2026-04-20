#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include "icss/core/runtime.hpp"
#include "icss/net/transport.hpp"
#include "icss/view/ascii_tactical_view.hpp"

#if !defined(_WIN32)
#include <csignal>
#endif

namespace {

std::atomic_bool g_shutdown_requested {false};

struct CliOptions {
    icss::net::BackendKind backend {icss::net::BackendKind::InProcess};
    icss::core::SampleMode sample_mode {icss::core::SampleMode::TrackedIntercept};
    std::uint64_t tick_limit {3};
    std::uint64_t tick_sleep_ms {0};
    bool run_forever {false};
    std::optional<std::string> repo_root;
    std::optional<std::string> bind_host;
    std::optional<std::string> tcp_frame_format;
    std::optional<int> tcp_port;
    std::optional<int> udp_port;
    std::optional<int> tick_rate_hz;
    std::optional<int> telemetry_interval_ms;
    std::optional<int> heartbeat_interval_ms;
    std::optional<int> heartbeat_timeout_ms;
    std::optional<int> udp_max_batch_snapshots;
    std::optional<bool> udp_send_latest_only;
    std::optional<int> max_clients;
};

[[noreturn]] void throw_usage(std::string_view message) {
    throw std::runtime_error(std::string(message));
}

bool parse_bool_arg(std::string_view value) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw_usage("boolean flag value must be true or false");
}

icss::net::BackendKind parse_backend(std::string_view value) {
    if (value == "in_process") {
        return icss::net::BackendKind::InProcess;
    }
    if (value == "socket_live") {
        return icss::net::BackendKind::SocketLive;
    }
    throw_usage("unsupported backend: " + std::string(value));
}

icss::core::SampleMode parse_sample_mode(std::string_view value) {
    if (value == "tracked_intercept") {
        return icss::core::SampleMode::TrackedIntercept;
    }
    if (value == "unguided_intercept") {
        return icss::core::SampleMode::UnguidedIntercept;
    }
    throw_usage("unsupported sample mode: " + std::string(value));
}

#if !defined(_WIN32)
void signal_handler(int) {
    g_shutdown_requested.store(true);
}

struct SignalHandlerScope {
    using Handler = void (*)(int);
    Handler old_sigint {SIG_DFL};
    Handler old_sigterm {SIG_DFL};

    SignalHandlerScope() {
        old_sigint = std::signal(SIGINT, signal_handler);
        old_sigterm = std::signal(SIGTERM, signal_handler);
    }

    ~SignalHandlerScope() {
        std::signal(SIGINT, old_sigint);
        std::signal(SIGTERM, old_sigterm);
    }
};
#else
struct SignalHandlerScope {};
#endif

CliOptions parse_args(int argc, char** argv) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string arg {argv[index]};
        if (arg == "--run-forever") {
            options.run_forever = true;
            continue;
        }
        if (arg == "--backend") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --backend");
            }
            options.backend = parse_backend(argv[++index]);
            continue;
        }
        if (arg == "--sample-mode") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --sample-mode");
            }
            options.sample_mode = parse_sample_mode(argv[++index]);
            continue;
        }
        if (arg == "--tick-limit") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --tick-limit");
            }
            options.tick_limit = std::stoull(argv[++index]);
            continue;
        }
        if (arg == "--tick-sleep-ms") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --tick-sleep-ms");
            }
            options.tick_sleep_ms = std::stoull(argv[++index]);
            continue;
        }
        if (arg == "--repo-root") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --repo-root");
            }
            options.repo_root = argv[++index];
            continue;
        }
        if (arg == "--bind-host") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --bind-host");
            }
            options.bind_host = argv[++index];
            continue;
        }
        if (arg == "--tcp-frame-format") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --tcp-frame-format");
            }
            options.tcp_frame_format = argv[++index];
            continue;
        }
        if (arg == "--tcp-port") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --tcp-port");
            }
            options.tcp_port = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--udp-port") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --udp-port");
            }
            options.udp_port = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--tick-rate-hz") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --tick-rate-hz");
            }
            options.tick_rate_hz = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--telemetry-interval-ms") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --telemetry-interval-ms");
            }
            options.telemetry_interval_ms = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--heartbeat-interval-ms") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --heartbeat-interval-ms");
            }
            options.heartbeat_interval_ms = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--heartbeat-timeout-ms") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --heartbeat-timeout-ms");
            }
            options.heartbeat_timeout_ms = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--udp-max-batch-snapshots") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --udp-max-batch-snapshots");
            }
            options.udp_max_batch_snapshots = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--udp-send-latest-only") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --udp-send-latest-only");
            }
            options.udp_send_latest_only = parse_bool_arg(argv[++index]);
            continue;
        }
        if (arg == "--max-clients") {
            if (index + 1 >= argc) {
                throw_usage("missing value for --max-clients");
            }
            options.max_clients = std::stoi(argv[++index]);
            continue;
        }
        if (arg == "--help") {
            throw_usage("usage: icss_server [--backend in_process|socket_live] [--sample-mode tracked_intercept|unguided_intercept] [--tick-limit N] [--tick-sleep-ms N] [--run-forever] [--repo-root PATH] [--bind-host IPv4] [--tcp-port N] [--udp-port N] [--tick-rate-hz N] [--telemetry-interval-ms N] [--heartbeat-interval-ms N] [--heartbeat-timeout-ms N] [--udp-max-batch-snapshots N] [--udp-send-latest-only true|false] [--max-clients N] [--tcp-frame-format json|binary]");
        }
        throw_usage("unknown argument: " + arg);
    }
    return options;
}

int run_in_process(const std::filesystem::path& repo_root,
                   const icss::core::RuntimeConfig& config,
                   icss::core::SampleMode sample_mode) {
    using namespace icss::core;
    BaselineRuntime runtime(config, sample_mode);
    const auto result = runtime.run();
    const auto& summary = result.summary;
    std::cout << "Interceptor Command Simulation System - server baseline (in-process authoritative runtime)\n";
    std::cout << "backend=in_process"
              << ", bind_host=" << result.config.server.bind_host
              << ", tcp_port=" << result.config.server.tcp_port
              << ", udp_port=" << result.config.server.udp_port
              << ", frame_format=" << result.config.server.tcp_frame_format << '\n';
    std::cout << "tick_rate_hz=" << result.config.server.tick_rate_hz
              << ", telemetry_interval_ms=" << result.config.scenario.telemetry_interval_ms
              << ", heartbeat_interval_ms=" << result.config.server.heartbeat_interval_ms
              << ", heartbeat_timeout_ms=" << result.config.server.heartbeat_timeout_ms << '\n';
    std::cout << "session_id=" << summary.session_id
              << ", phase=" << to_string(summary.phase)
              << ", snapshots=" << summary.snapshot_count
              << ", events=" << summary.event_count
              << ", resilience=" << summary.resilience_case << '\n';
    std::cout << "fire_control_console_connection=" << to_string(summary.fire_control_console_connection)
              << ", display_connection=" << to_string(summary.display_connection)
              << ", last_event_type=" << (summary.has_last_event ? icss::protocol::to_string(summary.last_event_type) : "none") << '\n';
    std::cout << "scenario=" << result.config.scenario.name
              << ", sample_mode=" << (sample_mode == SampleMode::TrackedIntercept ? "tracked_intercept" : "unguided_intercept")
              << ", latest_outputs=" << (repo_root / result.config.logging.aar_output_dir)
              << ", log_file=" << (repo_root / result.config.logging.file_path) << '\n';
    std::cout << "AAR artifacts written to " << (repo_root / result.config.logging.aar_output_dir) << '\n';
    return summary.assessment_ready ? 0 : 1;
}

int run_socket_live(const std::filesystem::path& repo_root,
                    const icss::core::RuntimeConfig& config,
                    std::uint64_t tick_limit,
                    std::uint64_t tick_sleep_ms,
                    bool run_forever) {
    using namespace icss::core;
    using namespace icss::net;

    g_shutdown_requested.store(false);
    SignalHandlerScope signal_scope {};
    auto transport = make_transport(BackendKind::SocketLive, config);
    const auto info = transport->info();
    std::cout << "Interceptor Command Simulation System - live server mode\n";
    std::cout << "backend=" << transport->backend_name()
              << ", bind_host=" << config.server.bind_host
              << ", tcp_port=" << info.tcp_port
              << ", udp_port=" << info.udp_port
              << ", frame_format=" << config.server.tcp_frame_format
              << ", binds_network=" << (info.binds_network ? "true" : "false") << '\n';
    std::cout << "tick_limit=" << tick_limit
              << ", tick_sleep_ms=" << tick_sleep_ms
              << ", run_forever=" << (run_forever ? "true" : "false")
              << ", tick_rate_hz=" << config.server.tick_rate_hz
              << ", telemetry_interval_ms=" << config.scenario.telemetry_interval_ms
              << ", heartbeat_interval_ms=" << config.server.heartbeat_interval_ms
              << ", heartbeat_timeout_ms=" << config.server.heartbeat_timeout_ms << '\n';
    std::cout << "udp_send_latest_only=" << (config.server.udp_send_latest_only ? "true" : "false")
              << ", udp_max_batch_snapshots=" << config.server.udp_max_batch_snapshots
              << ", max_clients=" << config.server.max_clients << '\n';
    std::cout << "startup_ready=true\n" << std::flush;
    if (run_forever) {
        while (!g_shutdown_requested.load()) {
            transport->poll_once();
            transport->advance_tick();
            if (tick_sleep_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tick_sleep_ms));
            }
        }
    } else {
        for (std::uint64_t tick = 0; tick < tick_limit; ++tick) {
            transport->poll_once();
            transport->advance_tick();
            if (tick_sleep_ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(tick_sleep_ms));
            }
        }
    }

    if (g_shutdown_requested.load()) {
        const auto pre_summary = transport->summary();
        if (pre_summary.phase != SessionPhase::Archived && (pre_summary.snapshot_count > 0 || pre_summary.event_count > 0)) {
            transport->archive_session();
        }
    }

    const auto summary = transport->summary();
    std::cout << "shutdown_requested=" << (g_shutdown_requested.load() ? "true" : "false") << '\n';
    std::cout << "shutdown_reason=" << (g_shutdown_requested.load() ? "signal" : "tick_limit") << '\n';
    std::cout << "phase=" << to_string(summary.phase)
              << ", snapshots=" << summary.snapshot_count
              << ", events=" << summary.event_count
              << ", resilience=" << summary.resilience_case << '\n';
    std::cout << "fire_control_console_connection=" << to_string(summary.fire_control_console_connection)
              << ", display_connection=" << to_string(summary.display_connection)
              << ", last_event_type=" << (summary.has_last_event ? icss::protocol::to_string(summary.last_event_type) : "none") << '\n';
    const auto latest_snapshot = summary.snapshot_count > 0
        ? std::optional<Snapshot> {transport->latest_snapshot()}
        : std::nullopt;
    write_runtime_session_log(config,
                              transport->backend_name(),
                              summary,
                              transport->events(),
                              latest_snapshot ? &*latest_snapshot : nullptr);
    if (summary.snapshot_count == 0) {
        std::cout << "AAR artifacts skipped because no snapshots were emitted\n";
        std::cout << "display_connection=disconnected, latest_freshness=stale, latest_snapshot_sequence=none\n";
        return 0;
    }

    const auto snapshot = *latest_snapshot;
    const auto aar_dir = repo_root / config.logging.aar_output_dir;
    const auto example_output = repo_root / "examples/sample-output.md";
    transport->write_aar_artifacts(aar_dir);
    transport->write_example_output(example_output, icss::view::make_replay_cursor(summary.event_count, summary.event_count == 0 ? 0 : summary.event_count - 1));
    std::cout << "display_connection=" << to_string(snapshot.display_connection)
              << ", latest_freshness=" << icss::view::freshness_label(snapshot)
              << ", latest_snapshot_sequence=" << snapshot.header.snapshot_sequence << '\n';
    std::cout << "AAR artifacts written to " << aar_dir << '\n';
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    using namespace icss::net;

    try {
        const auto options = parse_args(argc, argv);
        const fs::path repo_root = options.repo_root.has_value() ? fs::path{*options.repo_root} : fs::path{ICSS_REPO_ROOT};
        auto config = icss::core::default_runtime_config(repo_root);
        if (options.bind_host.has_value()) {
            config.server.bind_host = *options.bind_host;
        }
        if (options.tcp_frame_format.has_value()) {
            config.server.tcp_frame_format = *options.tcp_frame_format;
        }
        if (options.tcp_port.has_value()) {
            config.server.tcp_port = *options.tcp_port;
        }
        if (options.udp_port.has_value()) {
            config.server.udp_port = *options.udp_port;
        }
        if (options.tick_rate_hz.has_value()) {
            config.server.tick_rate_hz = *options.tick_rate_hz;
        }
        if (options.telemetry_interval_ms.has_value()) {
            config.scenario.telemetry_interval_ms = *options.telemetry_interval_ms;
        }
        if (options.heartbeat_interval_ms.has_value()) {
            config.server.heartbeat_interval_ms = *options.heartbeat_interval_ms;
        }
        if (options.heartbeat_timeout_ms.has_value()) {
            config.server.heartbeat_timeout_ms = *options.heartbeat_timeout_ms;
        }
        if (options.udp_max_batch_snapshots.has_value()) {
            config.server.udp_max_batch_snapshots = *options.udp_max_batch_snapshots;
        }
        if (options.udp_send_latest_only.has_value()) {
            config.server.udp_send_latest_only = *options.udp_send_latest_only;
        }
        if (options.max_clients.has_value()) {
            config.server.max_clients = *options.max_clients;
        }
        icss::core::validate_runtime_config(config);
        if (options.backend == BackendKind::InProcess) {
            return run_in_process(repo_root, config, options.sample_mode);
        }
        return run_socket_live(repo_root, config, options.tick_limit, options.tick_sleep_ms, options.run_forever);
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
