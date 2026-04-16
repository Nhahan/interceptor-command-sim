#pragma once

#include <filesystem>

#include "icss/core/config.hpp"
#include "icss/core/simulation.hpp"

namespace icss::core {

struct RuntimeResult {
    RuntimeConfig config;
    SessionSummary summary;
};

class BaselineRuntime {
public:
    explicit BaselineRuntime(RuntimeConfig config);

    [[nodiscard]] const RuntimeConfig& config() const;
    [[nodiscard]] RuntimeResult run() const;

private:
    RuntimeConfig config_;
};

RuntimeConfig default_runtime_config(const std::filesystem::path& repo_root);
SimulationSession run_baseline_demo(const RuntimeConfig& config);

}  // namespace icss::core
