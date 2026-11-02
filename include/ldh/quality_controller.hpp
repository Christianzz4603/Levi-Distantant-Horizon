#pragma once
#include <cstddef>
#include <cstdint>
namespace ldh {
struct QualityControllerConfig {
    double target_frame_ms{16.67};
    double ema_alpha{0.12};
    double degrade_ratio{1.15};
    double recover_ratio{0.78};
    std::uint32_t degrade_after_frames{12U};
    std::uint32_t recover_after_frames{90U};
    std::uint32_t max_sample_step{16U};
    std::size_t base_jobs_per_update{4U};
};
struct QualityState {
    double average_frame_ms{};
    std::uint32_t adaptive_sample_step{1U};
    std::uint32_t overloaded_frames{};
    std::uint32_t headroom_frames{};
    std::uint64_t observed_frames{};
};
class QualityController final {
public:
    explicit QualityController(QualityControllerConfig config = {});
    void observe_frame(double frame_ms) noexcept;
    [[nodiscard]] std::uint32_t sample_step_for_distance(double distance_blocks) const noexcept;
    [[nodiscard]] std::size_t recommended_jobs_per_update() const noexcept;
    [[nodiscard]] QualityState state() const noexcept;
private:
    QualityControllerConfig config_;
    QualityState state_{};
    bool has_average_{};
};
} // namespace ldh
