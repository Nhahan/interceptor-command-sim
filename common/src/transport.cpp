#include "icss/net/transport.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "icss/protocol/frame_codec.hpp"
#include "icss/protocol/serialization.hpp"
#include "icss/view/ascii_tactical_view.hpp"

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace icss::net {
namespace {

constexpr std::uint32_t kDefaultSessionId = 1001U;
constexpr std::string_view kTargetId = "target-alpha";
constexpr std::string_view kAssetId = "asset-interceptor";

icss::protocol::FrameFormat parse_frame_format(std::string_view value) {
    if (value == "json") {
        return icss::protocol::FrameFormat::JsonLine;
    }
    if (value == "binary") {
        return icss::protocol::FrameFormat::Binary;
    }
    throw std::runtime_error("unsupported tcp_frame_format: " + std::string(value));
}

std::vector<icss::core::EventRecord> recent_events(const icss::core::SimulationSession& session) {
    std::vector<icss::core::EventRecord> recent;
    const auto& events = session.events();
    const auto start = events.size() > 4 ? events.size() - 4 : 0;
    for (std::size_t index = start; index < events.size(); ++index) {
        recent.push_back(events[index]);
    }
    return recent;
}

icss::protocol::SnapshotPayload to_snapshot_payload(const icss::core::Snapshot& snapshot) {
    return {
        snapshot.envelope,
        snapshot.header,
        snapshot.target.id,
        snapshot.asset.id,
        snapshot.track.active,
        snapshot.track.confidence_pct,
        icss::core::to_string(snapshot.asset_status),
        icss::core::to_string(snapshot.command_status),
        snapshot.judgment.ready,
        icss::core::to_string(snapshot.judgment.code),
    };
}

icss::protocol::TelemetryPayload to_telemetry_payload(const icss::core::Snapshot& snapshot) {
    return {
        snapshot.envelope,
        snapshot.telemetry,
        icss::core::to_string(snapshot.viewer_connection),
    };
}

class InProcessTransport final : public TransportBackend {
public:
    explicit InProcessTransport(const icss::core::RuntimeConfig& config)
        : session_(kDefaultSessionId, config.server.tick_rate_hz, config.scenario.telemetry_interval_ms),
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

    icss::core::CommandResult start_scenario() override {
        return session_.start_scenario();
    }

    icss::core::CommandResult dispatch(const icss::protocol::TrackRequestPayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Track request rejected", {})) {
            return *invalid;
        }
        if (payload.target_id != kTargetId) {
            return reject_payload("Track request rejected",
                                  "track request target does not match the active target",
                                  {std::string{kTargetId}});
        }
        return session_.request_track();
    }

    icss::core::CommandResult dispatch(const icss::protocol::AssetActivatePayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Asset activation rejected", {})) {
            return *invalid;
        }
        if (payload.asset_id != kAssetId) {
            return reject_payload("Asset activation rejected",
                                  "asset activation does not match the configured asset",
                                  {std::string{kAssetId}});
        }
        return session_.activate_asset();
    }

    icss::core::CommandResult dispatch(const icss::protocol::CommandIssuePayload& payload) override {
        if (const auto invalid = validate_envelope(payload.envelope, command_sender_id_, "Command rejected", {})) {
            return *invalid;
        }
        if (payload.asset_id != kAssetId || payload.target_id != kTargetId) {
            return reject_payload("Command rejected",
                                  "command payload entity ids do not match the active session",
                                  {std::string{kAssetId}, std::string{kTargetId}});
        }
        return session_.issue_command();
    }

    void advance_tick() override {
        session_.advance_tick();
    }

    void archive_session() override {
        session_.archive_session();
    }

    [[nodiscard]] icss::core::Snapshot latest_snapshot() const override {
        return session_.latest_snapshot();
    }

    [[nodiscard]] std::vector<icss::core::EventRecord> events() const override {
        return session_.events();
    }

    [[nodiscard]] std::vector<icss::core::Snapshot> snapshots() const override {
        return session_.snapshots();
    }

    [[nodiscard]] icss::core::SessionSummary summary() const override {
        return session_.build_summary();
    }

    void write_aar_artifacts(const std::filesystem::path& output_dir) const override {
        session_.write_aar_artifacts(output_dir);
    }

    void write_example_output(const std::filesystem::path& output_file,
                              const icss::view::ReplayCursor& cursor) const override {
        std::filesystem::create_directories(output_file.parent_path());
        std::ofstream out(output_file);
        out << "# Sample Output\n\n```text\n";
        out << icss::view::render_tactical_frame(session_.latest_snapshot(), recent_events(session_), cursor);
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
        if (envelope.session_id != kDefaultSessionId) {
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

#if !defined(_WIN32)
class SocketHandle {
public:
    SocketHandle() = default;
    explicit SocketHandle(int fd) : fd_(fd) {}
    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;
    SocketHandle(SocketHandle&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    SocketHandle& operator=(SocketHandle&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    ~SocketHandle() { reset(); }

    [[nodiscard]] int get() const { return fd_; }
    [[nodiscard]] bool valid() const { return fd_ >= 0; }

private:
    void reset() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int fd_ {-1};
};

void set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        throw std::runtime_error("failed to set nonblocking socket");
    }
}

sockaddr_in make_addr(const std::string& host, std::uint16_t port) {
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        throw std::runtime_error("invalid IPv4 bind_host: " + host);
    }
    return addr;
}

std::uint16_t bound_port(int fd) {
    sockaddr_in addr {};
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        throw std::runtime_error("getsockname failed");
    }
    return ntohs(addr.sin_port);
}

bool same_endpoint(const sockaddr_in& lhs, const sockaddr_in& rhs) {
    return lhs.sin_family == rhs.sin_family
        && lhs.sin_port == rhs.sin_port
        && lhs.sin_addr.s_addr == rhs.sin_addr.s_addr;
}

SocketHandle bind_tcp_listener(const icss::core::ServerConfig& config) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("failed to create tcp socket");
    }
    SocketHandle handle(fd);
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    const auto addr = make_addr(config.bind_host, static_cast<std::uint16_t>(config.tcp_port));
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error(std::string("tcp bind failed: ") + std::strerror(errno));
    }
    if (::listen(fd, config.max_clients) != 0) {
        throw std::runtime_error(std::string("tcp listen failed: ") + std::strerror(errno));
    }
    set_nonblocking(fd);
    return handle;
}

SocketHandle bind_udp_socket(const icss::core::ServerConfig& config) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        throw std::runtime_error("failed to create udp socket");
    }
    SocketHandle handle(fd);
    const auto addr = make_addr(config.bind_host, static_cast<std::uint16_t>(config.udp_port));
    if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
        throw std::runtime_error(std::string("udp bind failed: ") + std::strerror(errno));
    }
    set_nonblocking(fd);
    return handle;
}

std::string payload_kind(std::string_view wire) {
    const auto prefix = std::string_view{"kind="};
    if (!wire.starts_with(prefix)) {
        throw std::runtime_error("wire payload missing kind prefix");
    }
    const auto end = wire.find(';');
    return std::string(wire.substr(prefix.size(), end == std::string_view::npos ? std::string_view::npos : end - prefix.size()));
}

class SocketLiveTransport final : public TransportBackend {
public:
    explicit SocketLiveTransport(const icss::core::RuntimeConfig& config)
        : config_(config),
          session_(kDefaultSessionId, config.server.tick_rate_hz, config.scenario.telemetry_interval_ms),
          tcp_listener_(bind_tcp_listener(config.server)),
          udp_socket_(bind_udp_socket(config.server)),
          tcp_frame_format_(parse_frame_format(config.server.tcp_frame_format)),
          heartbeat_timeout_ticks_(std::max<std::uint64_t>(1U,
              static_cast<std::uint64_t>((config.server.heartbeat_timeout_ms + config.scenario.telemetry_interval_ms - 1)
                                         / config.scenario.telemetry_interval_ms))) {
        info_ = {"socket_live", true, bound_port(tcp_listener_.get()), bound_port(udp_socket_.get())};
    }

    [[nodiscard]] std::string_view backend_name() const override { return info_.name; }
    [[nodiscard]] BackendInfo info() const override { return info_; }

    void poll_once() override {
        accept_tcp_client();
        flush_tcp_writes();
        flush_udp_datagrams();
        receive_udp_datagrams();
        receive_tcp_messages();
        flush_tcp_writes();
        flush_udp_datagrams();
    }

    void connect_client(icss::core::ClientRole, std::uint32_t) override {}
    void disconnect_client(icss::core::ClientRole role, std::string reason) override {
        if (role == icss::core::ClientRole::CommandConsole) {
            command_client_.reset();
            tcp_buffer_.clear();
            pending_tcp_lines_.clear();
            if (command_sender_id_ != 0U) {
                session_.disconnect_client(role, std::move(reason));
                command_sender_id_ = 0U;
            }
        } else {
            const auto sender_id = viewer_sender_id_;
            clear_viewer_binding();
            if (sender_id != 0U) {
                session_.disconnect_client(role, std::move(reason));
            }
        }
    }
    void timeout_client(icss::core::ClientRole role, std::string reason) override {
        session_.timeout_client(role, std::move(reason));
        if (role == icss::core::ClientRole::TacticalViewer) {
            clear_viewer_binding();
        }
        send_pending_snapshots();
    }

    icss::core::CommandResult start_scenario() override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::TrackRequestPayload&) override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::AssetActivatePayload&) override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::CommandIssuePayload&) override { return unsupported(); }

    void advance_tick() override {
        ++logical_tick_;
        maybe_timeout_viewer();
        session_.advance_tick();
        send_pending_snapshots();
    }

    void archive_session() override {
        session_.archive_session();
        send_pending_snapshots();
    }

    [[nodiscard]] icss::core::Snapshot latest_snapshot() const override { return session_.latest_snapshot(); }
    [[nodiscard]] std::vector<icss::core::EventRecord> events() const override { return session_.events(); }
    [[nodiscard]] std::vector<icss::core::Snapshot> snapshots() const override { return session_.snapshots(); }
    [[nodiscard]] icss::core::SessionSummary summary() const override { return session_.build_summary(); }
    void write_aar_artifacts(const std::filesystem::path& output_dir) const override { session_.write_aar_artifacts(output_dir); }

    void write_example_output(const std::filesystem::path& output_file,
                              const icss::view::ReplayCursor& cursor) const override {
        std::filesystem::create_directories(output_file.parent_path());
        std::ofstream out(output_file);
        out << "# Sample Output\n\n```text\n";
        out << icss::view::render_tactical_frame(session_.latest_snapshot(), recent_events(session_), cursor);
        out << "```\n";
    }

private:
    struct AarResolution {
        icss::view::ReplayCursor cursor {};
        std::string control {"absolute"};
        std::uint64_t requested_index {0};
        bool clamped {false};
    };

    struct ViewerBinding {
        std::uint32_t sender_id {};
        sockaddr_in endpoint {};
        std::uint64_t last_heartbeat_tick {};
        std::size_t last_snapshot_sent_index {};
    };

    static icss::core::CommandResult unsupported() {
        return {false, "socket_live direct dispatch is unsupported; use the network exchange path", icss::protocol::TcpMessageKind::CommandAck};
    }

    std::optional<icss::core::CommandResult> validate_command_envelope(const icss::protocol::SessionEnvelope& envelope) {
        if (command_sender_id_ == 0U) {
            return reject_payload("Command rejected", "command sender is not connected");
        }
        if (envelope.session_id != kDefaultSessionId) {
            return reject_payload("Command rejected", "payload session_id does not match the active session");
        }
        if (envelope.sender_id != command_sender_id_) {
            return reject_payload("Command rejected", "payload sender_id does not match the connected command console");
        }
        return std::nullopt;
    }

    static icss::core::CommandResult reject_ack(std::string reason) {
        return {false, std::move(reason), icss::protocol::TcpMessageKind::CommandAck};
    }

    icss::core::CommandResult reject_payload(std::string summary,
                                             std::string reason,
                                             std::vector<std::string> entity_ids = {}) {
        return session_.record_transport_rejection(std::move(summary), std::move(reason), std::move(entity_ids));
    }

    void queue_tcp_line(std::string line) {
        pending_tcp_lines_.push_back(std::move(line));
    }

    void queue_tcp_frame(std::string_view kind, std::string_view payload) {
        if (tcp_frame_format_ == icss::protocol::FrameFormat::JsonLine) {
            queue_tcp_line(icss::protocol::encode_json_frame(kind, payload) + "\n");
            return;
        }
        const auto bytes = icss::protocol::encode_binary_frame(kind, payload);
        pending_tcp_lines_.emplace_back(bytes.begin(), bytes.end());
    }

    void flush_tcp_writes() {
        if (!command_client_.has_value()) {
            return;
        }
        while (!pending_tcp_lines_.empty()) {
            auto& line = pending_tcp_lines_.front();
            const auto sent = ::send(command_client_->get(), line.data(), line.size(), 0);
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                throw std::runtime_error(std::string("tcp send failed: ") + std::strerror(errno));
            }
            line.erase(0, static_cast<std::size_t>(sent));
            if (!line.empty()) {
                return;
            }
            pending_tcp_lines_.pop_front();
        }
    }

    struct PendingDatagram {
        std::string wire;
        sockaddr_in target {};
    };

    void queue_udp_datagram(std::string wire, const sockaddr_in& target) {
        pending_udp_messages_.push_back({std::move(wire), target});
    }

    void flush_udp_datagrams() {
        while (!pending_udp_messages_.empty()) {
            auto& datagram = pending_udp_messages_.front();
            const auto sent = ::sendto(udp_socket_.get(), datagram.wire.data(), datagram.wire.size(), 0,
                                       reinterpret_cast<const sockaddr*>(&datagram.target), sizeof(sockaddr_in));
            if (sent < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                throw std::runtime_error(std::string("udp sendto failed: ") + std::strerror(errno));
            }
            if (static_cast<std::size_t>(sent) != datagram.wire.size()) {
                throw std::runtime_error("udp datagram was only partially sent");
            }
            pending_udp_messages_.pop_front();
        }
    }

    void send_aar_response(const icss::protocol::SessionEnvelope& request_envelope,
                           const AarResolution& resolution) {
        if (!command_client_.has_value()) {
            return;
        }
        const auto summary = session_.build_summary();
        const auto& events = session_.events();
        std::string event_type = "none";
        std::string event_summary = "no event";
        if (!events.empty()) {
            const auto& event = events[resolution.cursor.index];
            event_type = std::string(icss::protocol::to_string(event.header.event_type));
            event_summary = event.summary;
        }
        const icss::protocol::AarResponsePayload payload {
            {request_envelope.session_id, icss::core::kServerSenderId, ++server_sequence_},
            resolution.cursor.index,
            resolution.control,
            resolution.requested_index,
            resolution.clamped,
            icss::core::to_string(summary.judgment_code),
            summary.resilience_case,
            resolution.cursor.total,
            std::move(event_type),
            std::move(event_summary),
        };
        queue_tcp_frame("aar_response", icss::protocol::serialize(payload));
    }

    void send_ack(const icss::protocol::SessionEnvelope& request_envelope, const icss::core::CommandResult& result) {
        if (!command_client_.has_value()) {
            return;
        }
        const icss::protocol::CommandAckPayload payload {
            {request_envelope.session_id, icss::core::kServerSenderId, ++server_sequence_},
            result.accepted,
            result.reason,
        };
        queue_tcp_frame("command_ack", icss::protocol::serialize(payload));
    }

    void accept_tcp_client() {
        while (true) {
            sockaddr_in addr {};
            socklen_t len = sizeof(addr);
            const int fd = ::accept(tcp_listener_.get(), reinterpret_cast<sockaddr*>(&addr), &len);
            if (fd < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                throw std::runtime_error(std::string("tcp accept failed: ") + std::strerror(errno));
            }
            set_nonblocking(fd);
            if (command_client_.has_value()) {
                ::close(fd);
                session_.record_transport_rejection(
                    "Connection rejected",
                    "socket_live allows only one active command console connection");
                continue;
            }
            command_client_.emplace(fd);
            return;
        }
    }

    void receive_tcp_messages() {
        if (!command_client_.has_value()) {
            return;
        }

        char buffer[512];
        while (true) {
            const auto received = ::recv(command_client_->get(), buffer, sizeof(buffer), 0);
            if (received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                throw std::runtime_error(std::string("tcp recv failed: ") + std::strerror(errno));
            }
            if (received == 0) {
                disconnect_client(icss::core::ClientRole::CommandConsole, "tcp command connection closed");
                break;
            }
            tcp_buffer_.append(buffer, static_cast<std::size_t>(received));
        }

        if (tcp_frame_format_ == icss::protocol::FrameFormat::JsonLine) {
            std::size_t newline_pos = 0;
            while ((newline_pos = tcp_buffer_.find('\n')) != std::string::npos) {
                const auto wire = tcp_buffer_.substr(0, newline_pos);
                tcp_buffer_.erase(0, newline_pos + 1);
                if (!wire.empty()) {
                    try {
                        process_tcp_wire(wire);
                    } catch (const std::exception& error) {
                        session_.record_transport_rejection("Transport frame rejected", error.what());
                    }
                }
            }
            return;
        }
        icss::protocol::FramedMessage frame;
        while (icss::protocol::try_decode_binary_frame(tcp_buffer_, frame)) {
            try {
                process_tcp_frame(frame);
            } catch (const std::exception& error) {
                session_.record_transport_rejection("Transport frame rejected", error.what());
            }
        }
    }

    void process_tcp_wire(const std::string& wire) {
        process_tcp_frame(icss::protocol::decode_json_frame(wire));
    }

    void process_tcp_frame(const icss::protocol::FramedMessage& frame) {
        const auto& kind = frame.kind;
        const auto& payload_wire = frame.payload;
        if (kind == "session_join") {
            const auto payload = icss::protocol::parse_session_join(payload_wire);
            if (payload.client_role != icss::core::to_string(icss::core::ClientRole::CommandConsole)) {
                send_ack(payload.envelope, reject_payload("Session join rejected",
                                                         "tcp session_join only supports command_console role"));
                return;
            }
            if (payload.envelope.session_id != kDefaultSessionId) {
                send_ack(payload.envelope, reject_payload("Session join rejected",
                                                         "session_join payload session_id mismatch"));
                return;
            }
            if (command_sender_id_ != 0U && payload.envelope.sender_id != command_sender_id_) {
                send_ack(payload.envelope, reject_payload("Session join rejected",
                                                         "socket_live allows only one active command console"));
                return;
            }
            command_sender_id_ = payload.envelope.sender_id;
            session_.connect_client(icss::core::ClientRole::CommandConsole, command_sender_id_);
            send_ack(payload.envelope, {true, "command console joined", icss::protocol::TcpMessageKind::CommandAck});
            return;
        }
        if (kind == "session_leave") {
            const auto payload = icss::protocol::parse_session_leave(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            send_ack(payload.envelope, {true, payload.reason, icss::protocol::TcpMessageKind::CommandAck});
            flush_tcp_writes();
            disconnect_client(icss::core::ClientRole::CommandConsole, payload.reason);
            return;
        }
        if (kind == "scenario_start") {
            const auto payload = icss::protocol::parse_scenario_start(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            if (payload.scenario_name != config_.scenario.name) {
                send_ack(payload.envelope, reject_payload("Scenario start rejected",
                                                         "scenario_start payload does not match configured scenario"));
                return;
            }
            const auto result = session_.start_scenario();
            send_ack(payload.envelope, result);
            send_pending_snapshots();
            return;
        }
        if (kind == "scenario_stop") {
            const auto payload = icss::protocol::parse_scenario_stop(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            if (session_.phase() == icss::core::SessionPhase::Initialized ||
                session_.phase() == icss::core::SessionPhase::Archived) {
                send_ack(payload.envelope,
                         reject_payload("Scenario stop rejected",
                                        "scenario_stop is only valid while an active scenario is running"));
                return;
            }
            session_.archive_session();
            send_ack(payload.envelope, {true, payload.reason, icss::protocol::TcpMessageKind::CommandAck});
            send_pending_snapshots();
            return;
        }
        if (kind == "track_request") {
            const auto payload = icss::protocol::parse_track_request(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            if (payload.target_id != kTargetId) {
                send_ack(payload.envelope,
                         reject_payload("Track request rejected",
                                        "track request target does not match the active target",
                                        {std::string{kTargetId}}));
                return;
            }
            const auto result = session_.request_track();
            send_ack(payload.envelope, result);
            send_pending_snapshots();
            return;
        }
        if (kind == "asset_activate") {
            const auto payload = icss::protocol::parse_asset_activate(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            if (payload.asset_id != kAssetId) {
                send_ack(payload.envelope,
                         reject_payload("Asset activation rejected",
                                        "asset activation does not match the configured asset",
                                        {std::string{kAssetId}}));
                return;
            }
            const auto result = session_.activate_asset();
            send_ack(payload.envelope, result);
            send_pending_snapshots();
            return;
        }
        if (kind == "command_issue") {
            const auto payload = icss::protocol::parse_command_issue(payload_wire);
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            if (payload.asset_id != kAssetId || payload.target_id != kTargetId) {
                send_ack(payload.envelope, reject_payload("Command rejected",
                                                         "command payload entity ids do not match the active session",
                                                         {std::string{kAssetId}, std::string{kTargetId}}));
                return;
            }
            const auto result = session_.issue_command();
            send_ack(payload.envelope, result);
            send_pending_snapshots();
            return;
        }
        if (kind == "aar_request") {
            icss::protocol::AarRequestPayload payload;
            try {
                payload = icss::protocol::parse_aar_request(payload_wire);
            } catch (const std::exception& error) {
                queue_tcp_frame("command_ack",
                                icss::protocol::serialize(icss::protocol::CommandAckPayload{
                                    {kDefaultSessionId, icss::core::kServerSenderId, ++server_sequence_},
                                    false,
                                    std::string{"aar_request rejected: "} + error.what(),
                                }));
                return;
            }
            if (const auto invalid = validate_command_envelope(payload.envelope)) {
                send_ack(payload.envelope, *invalid);
                return;
            }
            const auto resolution = resolve_replay_cursor(payload);
            replay_cursor_ = resolution.cursor;
            send_aar_response(payload.envelope, resolution);
            return;
        }
        throw std::runtime_error("unsupported tcp payload kind: " + kind);
    }

    AarResolution resolve_replay_cursor(const icss::protocol::AarRequestPayload& payload) const {
        const auto total = session_.events().size();
        const bool use_existing_cursor = replay_cursor_.total == total && total != 0;
        const auto seed = use_existing_cursor
            ? replay_cursor_
            : icss::view::make_replay_cursor(total, payload.replay_cursor_index);
        AarResolution resolution {};
        resolution.control = payload.control;
        resolution.requested_index = payload.replay_cursor_index;
        if (payload.control == "absolute") {
            resolution.cursor = icss::view::jump_to(seed, payload.replay_cursor_index);
            resolution.clamped = total != 0 && payload.replay_cursor_index >= total;
            return resolution;
        }
        if (payload.control == "step_forward") {
            resolution.cursor = icss::view::step_forward(seed);
            resolution.clamped = (resolution.cursor.index == seed.index);
            return resolution;
        }
        if (payload.control == "step_backward") {
            resolution.cursor = icss::view::step_backward(seed);
            resolution.clamped = (resolution.cursor.index == seed.index);
            return resolution;
        }
        resolution.cursor = seed;
        resolution.clamped = true;
        return resolution;
    }

    void receive_udp_datagrams() {
        char buffer[512];
        sockaddr_in addr {};
        socklen_t len = sizeof(addr);
        while (true) {
            const auto received = ::recvfrom(udp_socket_.get(), buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&addr), &len);
            if (received < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                throw std::runtime_error(std::string("udp recvfrom failed: ") + std::strerror(errno));
            }
            const std::string wire(buffer, buffer + received);
            std::string kind;
            try {
                kind = payload_kind(wire);
            } catch (const std::exception&) {
                continue;
            }
            if (kind != "session_join") {
                if (kind == "viewer_heartbeat") {
                    icss::protocol::ViewerHeartbeatPayload payload;
                    try {
                        payload = icss::protocol::parse_viewer_heartbeat(wire);
                    } catch (const std::exception&) {
                        continue;
                    }
                    if (payload.envelope.session_id != kDefaultSessionId || payload.envelope.sender_id != viewer_sender_id_) {
                        continue;
                    }
                    last_viewer_heartbeat_tick_ = logical_tick_;
                    if (!session_.snapshots().empty() &&
                        session_.latest_snapshot().viewer_connection == icss::core::ConnectionState::TimedOut) {
                        session_.connect_client(icss::core::ClientRole::TacticalViewer, viewer_sender_id_);
                        send_pending_snapshots();
                    }
                    continue;
                }
                continue;
            }
            icss::protocol::SessionJoinPayload payload;
            try {
                payload = icss::protocol::parse_session_join(wire);
            } catch (const std::exception&) {
                continue;
            }
            if (payload.client_role != icss::core::to_string(icss::core::ClientRole::TacticalViewer)) {
                continue;
            }
            if (payload.envelope.session_id != kDefaultSessionId) {
                continue;
            }
            if (viewer_endpoint_.has_value() && viewer_sender_id_ != 0U &&
                payload.envelope.sender_id != viewer_sender_id_) {
                session_.record_transport_rejection(
                    "Viewer registration rejected",
                    "socket_live allows only one active tactical viewer");
                continue;
            }
            if (viewer_endpoint_.has_value() && viewer_sender_id_ == payload.envelope.sender_id &&
                same_endpoint(*viewer_endpoint_, addr)) {
                last_viewer_heartbeat_tick_ = logical_tick_;
                continue;
            }
            viewer_sender_id_ = payload.envelope.sender_id;
            viewer_endpoint_ = addr;
            last_viewer_heartbeat_tick_ = logical_tick_;
            reset_viewer_delivery_cursor();
            session_.connect_client(icss::core::ClientRole::TacticalViewer, viewer_sender_id_);
            send_pending_snapshots();
        }
    }

    void maybe_timeout_viewer() {
        if (!viewer_endpoint_.has_value() || !last_viewer_heartbeat_tick_.has_value()) {
            return;
        }
        if (logical_tick_ >= *last_viewer_heartbeat_tick_ + heartbeat_timeout_ticks_) {
            timeout_client(icss::core::ClientRole::TacticalViewer,
                           "viewer heartbeat timeout threshold exceeded");
            last_viewer_heartbeat_tick_.reset();
        }
    }

    void send_pending_snapshots() {
        if (!viewer_endpoint_.has_value()) {
            return;
        }
        const auto& snapshots = session_.snapshots();
        if (last_snapshot_sent_index_ >= snapshots.size()) {
            return;
        }

        if (config_.server.udp_send_latest_only && snapshots.size() - last_snapshot_sent_index_ > 1) {
            pending_udp_messages_.clear();
            const auto& snapshot = snapshots.back();
            queue_udp_datagram(icss::protocol::serialize(to_snapshot_payload(snapshot)), *viewer_endpoint_);
            queue_udp_datagram(icss::protocol::serialize(to_telemetry_payload(snapshot)), *viewer_endpoint_);
            last_snapshot_sent_index_ = snapshots.size();
            return;
        }

        int queued = 0;
        while (last_snapshot_sent_index_ < snapshots.size() &&
               queued < std::max(1, config_.server.udp_max_batch_snapshots)) {
            const auto& snapshot = snapshots[last_snapshot_sent_index_];
            queue_udp_datagram(icss::protocol::serialize(to_snapshot_payload(snapshot)), *viewer_endpoint_);
            queue_udp_datagram(icss::protocol::serialize(to_telemetry_payload(snapshot)), *viewer_endpoint_);
            ++last_snapshot_sent_index_;
            ++queued;
        }
    }

    void clear_viewer_binding() {
        viewer_endpoint_.reset();
        last_viewer_heartbeat_tick_.reset();
        pending_udp_messages_.clear();
        viewer_sender_id_ = 0U;
        last_snapshot_sent_index_ = 0U;
    }

    void reset_viewer_delivery_cursor() {
        const auto& snapshots = session_.snapshots();
        if (snapshots.empty()) {
            last_snapshot_sent_index_ = 0U;
            return;
        }
        const auto batch_limit = std::max(1, config_.server.udp_max_batch_snapshots);
        const auto rewind_count = config_.server.udp_send_latest_only
            ? std::size_t{1}
            : std::min<std::size_t>(snapshots.size(), static_cast<std::size_t>(batch_limit));
        last_snapshot_sent_index_ = snapshots.size() - rewind_count;
    }

    icss::core::RuntimeConfig config_;
    icss::core::SimulationSession session_;
    SocketHandle tcp_listener_;
    SocketHandle udp_socket_;
    std::optional<SocketHandle> command_client_;
    std::optional<sockaddr_in> viewer_endpoint_;
    std::optional<std::uint64_t> last_viewer_heartbeat_tick_;
    std::string tcp_buffer_;
    std::deque<std::string> pending_tcp_lines_;
    std::deque<PendingDatagram> pending_udp_messages_;
    std::uint32_t command_sender_id_ {0};
    std::uint32_t viewer_sender_id_ {0};
    std::uint64_t server_sequence_ {0};
    std::uint64_t logical_tick_ {0};
    icss::protocol::FrameFormat tcp_frame_format_ {icss::protocol::FrameFormat::JsonLine};
    std::uint64_t heartbeat_timeout_ticks_ {1};
    std::size_t last_snapshot_sent_index_ {0};
    icss::view::ReplayCursor replay_cursor_ {};
    BackendInfo info_ {};
};
#else
class SocketLiveTransport final : public TransportBackend {
public:
    explicit SocketLiveTransport(const icss::core::RuntimeConfig&) {}

    [[nodiscard]] std::string_view backend_name() const override { return "socket_live"; }
    [[nodiscard]] BackendInfo info() const override { return {"socket_live", false, 0, 0}; }
    void poll_once() override {}

    void connect_client(icss::core::ClientRole, std::uint32_t) override {}
    void disconnect_client(icss::core::ClientRole, std::string) override {}
    void timeout_client(icss::core::ClientRole, std::string) override {}

    icss::core::CommandResult start_scenario() override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::TrackRequestPayload&) override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::AssetActivatePayload&) override { return unsupported(); }
    icss::core::CommandResult dispatch(const icss::protocol::CommandIssuePayload&) override { return unsupported(); }
    void advance_tick() override {}
    void archive_session() override {}

    [[nodiscard]] icss::core::Snapshot latest_snapshot() const override { throw std::logic_error("socket live backend inactive"); }
    [[nodiscard]] std::vector<icss::core::EventRecord> events() const override { return {}; }
    [[nodiscard]] std::vector<icss::core::Snapshot> snapshots() const override { return {}; }
    [[nodiscard]] icss::core::SessionSummary summary() const override { return {}; }

    void write_aar_artifacts(const std::filesystem::path&) const override {}
    void write_example_output(const std::filesystem::path&, const icss::view::ReplayCursor&) const override {}

private:
    static icss::core::CommandResult unsupported() {
        return {false, "socket live backend requires POSIX sockets in this baseline", icss::protocol::TcpMessageKind::CommandAck};
    }
};
#endif

}  // namespace

std::unique_ptr<TransportBackend> make_transport(BackendKind kind, const icss::core::RuntimeConfig& config) {
    switch (kind) {
    case BackendKind::InProcess:
        return std::make_unique<InProcessTransport>(config);
    case BackendKind::SocketLive:
        return std::make_unique<SocketLiveTransport>(config);
    }
    throw std::logic_error("unsupported transport backend kind");
}

}  // namespace icss::net
