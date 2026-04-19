#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include "icss/core/config.hpp"
#include "icss/core/simulation.hpp"

namespace icss::core {

enum class SampleMode : std::uint8_t {
    Guided,
    Straight,
};

struct RuntimeResult {
    RuntimeConfig config;
    SessionSummary summary;
};

class BaselineRuntime {
public:
    explicit BaselineRuntime(RuntimeConfig config, SampleMode sample_mode = SampleMode::Guided);

    [[nodiscard]] const RuntimeConfig& config() const;
    [[nodiscard]] SampleMode sample_mode() const;
    [[nodiscard]] RuntimeResult run() const;

private:
    RuntimeConfig config_;
    SampleMode sample_mode_ {SampleMode::Guided};
};

RuntimeConfig default_runtime_config(const std::filesystem::path& repo_root);
SimulationSession run_baseline_demo(const RuntimeConfig& config, SampleMode sample_mode = SampleMode::Guided);
SimulationSession run_baseline_demo(SampleMode sample_mode);
SimulationSession run_baseline_demo();
void write_runtime_session_log(const RuntimeConfig& config,
                               std::string_view backend_name,
                               const SessionSummary& summary,
                               const std::vector<EventRecord>& events,
                               const Snapshot* latest_snapshot = nullptr);

}  // namespace icss::core
