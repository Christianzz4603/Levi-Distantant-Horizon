#pragma once

#include <cstddef>
#include <cstdint>

namespace ldh {

enum class PerformanceProfile : std::uint8_t {
    LowEnd,
    Balanced,
    Quality
};

struct PerformanceBudget {
    std::uint32_t sample_step{2U};
    std::size_t jobs_per_update{2U};
    std::size_t lod_radius_chunks{48U};
    std::size_t cache_budget_bytes{16U * 1024U * 1024U};
    std::size_t max_pending_jobs{4U};
    std::size_t max_input_samples{131072U};
    bool skip_optional_work{};
};

struct PerformanceGovernorConfig {
    PerformanceProfile profile{PerformanceProfile::Balanced};
    double target_frame_ms{33.33};
    double ema_alpha{0.15};
    double degrade_ratio{1.15};
    double recover_ratio{0.78};
    std::uint32_t degrade_after_frames{8U};
    std::uint32_t recover_after_frames{90U};
    std::uint32_t base_sample_step{2U};
    std::uint32_t max_sample_step{16U};
    std::size_t base_jobs_per_update{2U};
    std::size_t base_lod_radius_chunks{48U};
    std::size_t base_cache_budget_bytes{16U * 1024U * 1024U};
    std::size_t base_max_pending_jobs{4U};
    std::size_t max_input_samples{131072U};
};

struct PerformanceState {
    double average_frame_ms{};
    double memory_pressure{};
    std::uint32_t adaptive_sample_step{2U};
    std::uint32_t overloaded_frames{};
    std::uint32_t headroom_frames{};
    std::uint64_t observed_frames{};
};

class PerformanceGovernor final {
public:
    explicit PerformanceGovernor(PerformanceGovernorConfig config = {});

    [[nodiscard]] static PerformanceGovernorConfig config_for(
        PerformanceProfile profile) noexcept;

    void set_profile(PerformanceProfile profile) noexcept;
    void observe_frame(double frame_ms) noexcept;
    void observe_memory_usage(std::size_t used_bytes,
                              std::size_t available_budget_bytes) noexcept;

    [[nodiscard]] PerformanceBudget budget() const noexcept;
    [[nodiscard]] PerformanceState state() const noexcept;

private:
    void normalize() noexcept;

    PerformanceGovernorConfig config_;
    PerformanceState state_{};
    bool has_frame_average_{};
};

} // namespace ldh
