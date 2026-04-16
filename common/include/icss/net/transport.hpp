#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "icss/core/config.hpp"
#include "icss/core/simulation.hpp"
#include "icss/protocol/payloads.hpp"
#include "icss/view/replay_cursor.hpp"

namespace icss::net {

enum class BackendKind : std::uint8_t {
    InProcess,
    SocketLive,
};

struct BackendInfo {
    std::string_view name;
    bool binds_network {false};
    std::uint16_t tcp_port {0};
    std::uint16_t udp_port {0};
};

class TransportBackend {
public:
    virtual ~TransportBackend() = default;

    [[nodiscard]] virtual std::string_view backend_name() const = 0;
    [[nodiscard]] virtual BackendInfo info() const = 0;
    virtual void poll_once() = 0;
    virtual void connect_client(icss::core::ClientRole role, std::uint32_t sender_id) = 0;
    virtual void disconnect_client(icss::core::ClientRole role, std::string reason) = 0;
    virtual void timeout_client(icss::core::ClientRole role, std::string reason) = 0;

    virtual icss::core::CommandResult start_scenario() = 0;
    virtual icss::core::CommandResult dispatch(const icss::protocol::TrackRequestPayload& payload) = 0;
    virtual icss::core::CommandResult dispatch(const icss::protocol::AssetActivatePayload& payload) = 0;
    virtual icss::core::CommandResult dispatch(const icss::protocol::CommandIssuePayload& payload) = 0;
    virtual void advance_tick() = 0;
    virtual void archive_session() = 0;

    [[nodiscard]] virtual icss::core::Snapshot latest_snapshot() const = 0;
    [[nodiscard]] virtual std::vector<icss::core::EventRecord> events() const = 0;
    [[nodiscard]] virtual std::vector<icss::core::Snapshot> snapshots() const = 0;
    [[nodiscard]] virtual icss::core::SessionSummary summary() const = 0;

    virtual void write_aar_artifacts(const std::filesystem::path& output_dir) const = 0;
    virtual void write_example_output(const std::filesystem::path& output_file,
                                      const icss::view::ReplayCursor& cursor) const = 0;
};

std::unique_ptr<TransportBackend> make_transport(BackendKind kind, const icss::core::RuntimeConfig& config);

}  // namespace icss::net
