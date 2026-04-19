#include "transport_internal.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "icss/view/ascii_tactical_view.hpp"

namespace icss::net {
namespace {

class InProcessTransport final : public TransportBackend {
public:
    explicit InProcessTransport(const icss::core::RuntimeConfig& config)
        : session_(detail::kDefaultSessionId, config.server.tick_rate_hz, config.scenario.telemetry_interval_ms, config.scenario),
          info_{"in_process", false,
                static_cast<std::uint16_t>(config.server.tcp_port),
                static_cast<std::uint16_t>(config.server.udp_port)} {}

    [[nodiscard]] std::string_view backend_name() const override { return "in_process"; }
    [[nodiscard]] BackendInfo info() const override { return info_; }
    void poll_once() override {}

    void connect_client(icss::core::ClientRole role, std::uint32_t sender_id) override {
        if (role == icss::core::ClientRole::CommandConsole) {
            command_sender_id_ = sender_id;
        } else {
            viewer_sender_id_ = sender_id;
        }
        session_.connect_client(role, sender_id);
    }

    void disconnect_client(icss::core::ClientRole role, std::string reason) override {
        if (role == icss::core::ClientRole::CommandConsole) {
            command_sender_id_ = 0U;
        } else {
            viewer_sender_id_ = 0U;
        }
        session_.disconnect_client(role, std::move(reason));
    }

    void timeout_client(icss::core::ClientRole role, std::string reason) override {
        session_.timeout_client(role, std::move(reason));
    }

    icss::core::CommandResult start_scenario() override { return session_.start_scenario(); }
    icss::core::CommandResult reset_session(std::string reason) override { return session_.reset_session(std::move(reason)); }

    icss::core::CommandResult dispatch(const icss::protocol::TrackRequestPayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Track request rejected", {})) {
            return *invalid;
        }
        if (payload.target_id != detail::kTargetId) {
            return reject_payload("Track request rejected",
                                  "track request target does not match the active target",
                                  {std::string{detail::kTargetId}});
        }
        return session_.request_track();
    }

    icss::core::CommandResult dispatch(const icss::protocol::TrackReleasePayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Track release rejected", {})) {
            return *invalid;
        }
        if (payload.target_id != detail::kTargetId) {
            return reject_payload("Track release rejected",
                                  "track release target does not match the active target",
                                  {std::string{detail::kTargetId}});
        }
        return session_.release_track();
    }

    icss::core::CommandResult dispatch(const icss::protocol::AssetActivatePayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Asset activation rejected", {})) {
            return *invalid;
        }
        if (payload.asset_id != detail::kAssetId) {
            return reject_payload("Asset activation rejected",
                                  "asset activation does not match the configured asset",
                                  {std::string{detail::kAssetId}});
        }
        return session_.activate_asset();
    }

    icss::core::CommandResult dispatch(const icss::protocol::CommandIssuePayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Command rejected", {})) {
            return *invalid;
        }
        if (payload.asset_id != detail::kAssetId || payload.target_id != detail::kTargetId) {
            return reject_payload("Command rejected",
                                  "command payload entity ids do not match the active session",
                                  {std::string{detail::kAssetId}, std::string{detail::kTargetId}});
        }
        return session_.issue_command();
    }

    void advance_tick() override { session_.advance_tick(); }
    void archive_session() override { session_.archive_session(); }

    [[nodiscard]] icss::core::Snapshot latest_snapshot() const override { return session_.latest_snapshot(); }
    [[nodiscard]] std::vector<icss::core::EventRecord> events() const override { return session_.events(); }
    [[nodiscard]] std::vector<icss::core::Snapshot> snapshots() const override { return session_.snapshots(); }
    [[nodiscard]] icss::core::SessionSummary summary() const override { return session_.build_summary(); }

    void write_aar_artifacts(const std::filesystem::path& output_dir) const override { session_.write_aar_artifacts(output_dir); }

    void write_example_output(const std::filesystem::path& output_file,
                              const icss::view::ReplayCursor& cursor) const override {
        const auto parent = output_file.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream out(output_file);
        const auto summary = session_.build_summary();
        const auto snapshot = session_.latest_snapshot();
        const auto& events = session_.events();
        const auto guidance_event = std::find_if(events.rbegin(), events.rend(), [](const icss::core::EventRecord& event) {
            return event.header.event_type == icss::protocol::EventType::CommandAccepted
                || event.header.event_type == icss::protocol::EventType::TrackUpdated;
        });
        const bool guidance_active = guidance_event == events.rend()
            ? snapshot.track.active
            : !(guidance_event->summary.find("disabled") != std::string::npos
                || guidance_event->details.find("disabled") != std::string::npos
                || guidance_event->summary.find("straight") != std::string::npos
                || guidance_event->details.find("straight") != std::string::npos);
        out << "# Sample Output\n\n";
        out << "- schema_version: " << detail::kSampleOutputSchemaVersion << "\n";
        out << "- backend: in_process\n";
        out << "- session_id: " << summary.session_id << "\n";
        out << "- cursor_index: " << cursor.index << "/" << cursor.total << "\n";
        out << "- command_console_connection: " << icss::core::to_string(summary.command_console_connection) << "\n";
        out << "- viewer_connection: " << icss::core::to_string(summary.viewer_connection) << "\n";
        out << "- guidance_state: " << (guidance_active ? "on" : "off") << "\n";
        out << "- launch_mode: " << (guidance_active ? "guided" : "straight") << "\n";
        out << "- launch_angle_deg: " << static_cast<int>(snapshot.launch_angle_deg) << "\n";
        out << "- latest_freshness: " << icss::view::freshness_label(snapshot) << "\n";
        out << "- latest_snapshot_sequence: " << snapshot.header.snapshot_sequence << "\n";
        out << "- last_event_type: " << (summary.has_last_event ? icss::protocol::to_string(summary.last_event_type) : "none") << "\n";
        out << "- resilience_case: " << summary.resilience_case << "\n\n";
        out << "```text\n";
        out << icss::view::render_tactical_frame(snapshot, detail::recent_events(session_), cursor);
        out << "```\n";
    }

private:
    std::optional<icss::core::CommandResult> validate_envelope(
        const icss::protocol::SessionEnvelope& envelope,
        std::uint32_t expected_sender,
        std::string summary,
        std::vector<std::string> entity_ids) {
        if (expected_sender == 0U) {
            return reject_payload(std::move(summary), "command sender is not connected", std::move(entity_ids));
        }
        if (envelope.session_id != detail::kDefaultSessionId) {
            return reject_payload(std::move(summary), "payload session_id does not match the active session", std::move(entity_ids));
        }
        if (envelope.sender_id != expected_sender) {
            return reject_payload(std::move(summary), "payload sender_id does not match the connected command console", std::move(entity_ids));
        }
        return std::nullopt;
    }

    icss::core::CommandResult reject_payload(std::string summary,
                                             std::string reason,
                                             std::vector<std::string> entity_ids = {}) {
        return session_.record_transport_rejection(std::move(summary), std::move(reason), std::move(entity_ids));
    }

    icss::core::SimulationSession session_;
    std::uint32_t command_sender_id_ {0};
    std::uint32_t viewer_sender_id_ {0};
    BackendInfo info_ {};
};

}  // namespace

namespace detail {
std::unique_ptr<TransportBackend> make_inprocess_transport(const icss::core::RuntimeConfig& config) {
    return std::make_unique<InProcessTransport>(config);
}
}  // namespace detail

}  // namespace icss::net
