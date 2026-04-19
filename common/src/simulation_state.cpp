#include "icss/core/simulation.hpp"

#include <stdexcept>

#include "simulation_internal.hpp"

namespace icss::core {

SimulationSession::SimulationSession(std::uint32_t session_id,
                                     int tick_rate_hz,
                                     int telemetry_interval_ms,
                                     ScenarioConfig scenario)
    : session_id_(session_id),
      tick_rate_hz_(tick_rate_hz),
      telemetry_interval_ms_(telemetry_interval_ms),
      scenario_(std::move(scenario)),
      target_({"target-alpha", {scenario_.target_start_x, scenario_.target_start_y}, false}),
      asset_({"asset-interceptor", {scenario_.interceptor_start_x, scenario_.interceptor_start_y}, false}),
      target_world_({static_cast<float>(scenario_.target_start_x), static_cast<float>(scenario_.target_start_y)}),
      asset_world_({static_cast<float>(scenario_.interceptor_start_x), static_cast<float>(scenario_.interceptor_start_y)}),
      target_velocity_world_({static_cast<float>(scenario_.target_velocity_x), static_cast<float>(scenario_.target_velocity_y)}),
      asset_velocity_world_({0.0F, 0.0F}),
      seeker_lock_(false),
      off_boresight_deg_(0.0F) {}

void SimulationSession::clear_track_state() {
    track_ = {};
    track_estimate_world_ = {};
    track_estimate_velocity_ = {};
    track_measurement_world_ = {};
    track_position_variance_ = {};
    track_velocity_variance_ = {};
    track_measurement_valid_ = false;
    track_measurement_age_ticks_ = 0;
    track_missed_updates_ = 0;
}

void SimulationSession::seed_track_state_from_target() {
    track_.active = true;
    track_estimate_world_ = target_world_;
    track_estimate_velocity_ = target_velocity_world_;
    track_measurement_world_ = detail::add(target_world_, detail::deterministic_noise(tick_));
    track_measurement_valid_ = true;
    track_measurement_age_ticks_ = 0;
    track_missed_updates_ = 0;
    track_position_variance_ = {36.0F, 36.0F};
    track_velocity_variance_ = {12.0F, 12.0F};
    track_.estimated_position = track_estimate_world_;
    track_.estimated_velocity = track_estimate_velocity_;
    track_.measurement_position = track_measurement_world_;
    track_.measurement_valid = true;
    track_.measurement_residual_distance = detail::length(detail::subtract(track_measurement_world_, track_estimate_world_));
    track_.covariance_trace = detail::covariance_trace(track_position_variance_, track_velocity_variance_);
    track_.measurement_age_ticks = 0;
    track_.missed_updates = 0;
}

void SimulationSession::update_track_state() {
    track_estimate_world_ = detail::add(track_estimate_world_, track_estimate_velocity_);
    track_position_variance_.x += track_velocity_variance_.x + 4.0F;
    track_position_variance_.y += track_velocity_variance_.y + 4.0F;
    track_velocity_variance_.x += 1.5F;
    track_velocity_variance_.y += 1.5F;

    const auto measurement_due = detail::measurement_due(tick_);
    const auto fresh_measurement = detail::measurement_available(tick_);
    if (fresh_measurement) {
        track_measurement_world_ = detail::add(target_world_, detail::deterministic_noise(tick_));
        track_measurement_age_ticks_ = 0;
        track_missed_updates_ = 0;
        track_measurement_valid_ = true;
        const auto residual = detail::subtract(track_measurement_world_, track_estimate_world_);
        track_.measurement_residual_distance = detail::length(residual);
        constexpr float kMeasurementVariance = 25.0F;
        const auto kpx = track_position_variance_.x / (track_position_variance_.x + kMeasurementVariance);
        const auto kpy = track_position_variance_.y / (track_position_variance_.y + kMeasurementVariance);
        track_estimate_world_.x += kpx * residual.x;
        track_estimate_world_.y += kpy * residual.y;
        track_position_variance_.x *= (1.0F - kpx);
        track_position_variance_.y *= (1.0F - kpy);

        const auto kvx = track_velocity_variance_.x / (track_velocity_variance_.x + (kMeasurementVariance * 4.0F));
        const auto kvy = track_velocity_variance_.y / (track_velocity_variance_.y + (kMeasurementVariance * 4.0F));
        track_estimate_velocity_.x += kvx * residual.x;
        track_estimate_velocity_.y += kvy * residual.y;
        track_velocity_variance_.x *= (1.0F - kvx);
        track_velocity_variance_.y *= (1.0F - kvy);
    } else {
        ++track_measurement_age_ticks_;
        if (measurement_due) {
            ++track_missed_updates_;
        }
    }

    track_.estimated_position = track_estimate_world_;
    track_.estimated_velocity = track_estimate_velocity_;
    track_.measurement_position = track_measurement_world_;
    track_.measurement_valid = track_measurement_valid_;
    track_.measurement_age_ticks = track_measurement_age_ticks_;
    track_.missed_updates = track_missed_updates_;
    track_.covariance_trace = detail::covariance_trace(track_position_variance_, track_velocity_variance_);
}

void SimulationSession::reset_world_state_from_scenario() {
    target_world_ = {static_cast<float>(scenario_.target_start_x), static_cast<float>(scenario_.target_start_y)};
    asset_world_ = {static_cast<float>(scenario_.interceptor_start_x), static_cast<float>(scenario_.interceptor_start_y)};
    target_velocity_world_ = {static_cast<float>(scenario_.target_velocity_x), static_cast<float>(scenario_.target_velocity_y)};
    asset_velocity_world_ = {0.0F, 0.0F};
    seeker_lock_ = false;
    off_boresight_deg_ = 0.0F;
    target_ = {"target-alpha", {scenario_.target_start_x, scenario_.target_start_y}, false};
    asset_ = {"asset-interceptor", {scenario_.interceptor_start_x, scenario_.interceptor_start_y}, false};
}

std::uint64_t SimulationSession::next_timestamp_ms() {
    clock_ms_ += static_cast<std::uint64_t>(telemetry_interval_ms_);
    return clock_ms_;
}

ClientState& SimulationSession::client(ClientRole role) {
    return role == ClientRole::CommandConsole ? command_console_ : tactical_viewer_;
}

const ClientState& SimulationSession::client(ClientRole role) const {
    return role == ClientRole::CommandConsole ? command_console_ : tactical_viewer_;
}

void SimulationSession::push_event(protocol::EventType type,
                                   std::string source,
                                   std::vector<std::string> entity_ids,
                                   std::string summary,
                                   std::string details) {
    events_.push_back({{tick_, next_timestamp_ms(), type}, std::move(source), std::move(entity_ids), std::move(summary), std::move(details)});
}

CommandResult SimulationSession::reject_command(std::string summary,
                                                std::string reason,
                                                std::vector<std::string> entity_ids) {
    push_event(protocol::EventType::CommandRejected,
               "simulation_server",
               std::move(entity_ids),
               std::move(summary),
               reason);
    if (!judgment_.ready) {
        command_status_ = CommandLifecycle::Rejected;
        judgment_.code = JudgmentCode::InvalidTransition;
        judgment_.summary = reason;
    }
    return {false, std::move(reason), protocol::TcpMessageKind::CommandAck};
}

SessionPhase SimulationSession::phase() const {
    return phase_;
}

const std::vector<EventRecord>& SimulationSession::events() const {
    return events_;
}

const std::vector<Snapshot>& SimulationSession::snapshots() const {
    return snapshots_;
}

Snapshot SimulationSession::latest_snapshot() const {
    if (snapshots_.empty()) {
        throw std::runtime_error("no snapshots recorded yet");
    }
    return snapshots_.back();
}

}  // namespace icss::core
