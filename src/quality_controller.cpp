#include "ldh/quality_controller.hpp"
#include <algorithm>
#include <cmath>
namespace ldh {
QualityController::QualityController(QualityControllerConfig config) : config_(config) {
    if (!std::isfinite(config_.target_frame_ms) || config_.target_frame_ms <= 0.0) config_.target_frame_ms = 16.67;
    if (!std::isfinite(config_.ema_alpha)) config_.ema_alpha = 0.12;
    config_.ema_alpha = std::clamp(config_.ema_alpha, 0.01, 1.0);
    if (!std::isfinite(config_.degrade_ratio) || config_.degrade_ratio <= 1.0) config_.degrade_ratio = 1.15;
    if (!std::isfinite(config_.recover_ratio) || config_.recover_ratio <= 0.0 || config_.recover_ratio >= 1.0) config_.recover_ratio = 0.78;
    config_.degrade_after_frames = std::max(1U, config_.degrade_after_frames);
    config_.recover_after_frames = std::max(1U, config_.recover_after_frames);
    config_.max_sample_step = std::max(1U, config_.max_sample_step);
    config_.base_jobs_per_update = std::max<std::size_t>(1U, config_.base_jobs_per_update);
}
void QualityController::observe_frame(double frame_ms) noexcept {
    if (!std::isfinite(frame_ms) || frame_ms <= 0.0) return;
    if (!has_average_) { state_.average_frame_ms = frame_ms; has_average_ = true; }
    else state_.average_frame_ms += config_.ema_alpha * (frame_ms - state_.average_frame_ms);
    ++state_.observed_frames;
    if (state_.average_frame_ms > config_.target_frame_ms * config_.degrade_ratio) {
        state_.headroom_frames = 0U;
        if (state_.overloaded_frames < config_.degrade_after_frames) ++state_.overloaded_frames;
        if (state_.overloaded_frames >= config_.degrade_after_frames) {
            state_.adaptive_sample_step = std::min(config_.max_sample_step,
                state_.adaptive_sample_step > config_.max_sample_step / 2U
                    ? config_.max_sample_step : state_.adaptive_sample_step * 2U);
            state_.overloaded_frames = 0U;
        }
    } else if (state_.average_frame_ms < config_.target_frame_ms * config_.recover_ratio) {
        state_.overloaded_frames = 0U;
        if (state_.headroom_frames < config_.recover_after_frames) ++state_.headroom_frames;
        if (state_.headroom_frames >= config_.recover_after_frames) {
            state_.adaptive_sample_step = std::max(1U, state_.adaptive_sample_step / 2U);
            state_.headroom_frames = 0U;
        }
    } else { state_.overloaded_frames = 0U; state_.headroom_frames = 0U; }
}
std::uint32_t QualityController::sample_step_for_distance(double distance_blocks) const noexcept {
    if (!std::isfinite(distance_blocks) || distance_blocks < 0.0) return config_.max_sample_step;
    std::uint32_t distance_step = 1U;
    if (distance_blocks > 256.0) distance_step = 8U;
    else if (distance_blocks > 128.0) distance_step = 4U;
    else if (distance_blocks > 64.0) distance_step = 2U;
    const auto combined = static_cast<std::uint64_t>(distance_step) * state_.adaptive_sample_step;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(combined, config_.max_sample_step));
}
std::size_t QualityController::recommended_jobs_per_update() const noexcept {
    return std::max<std::size_t>(1U, config_.base_jobs_per_update / state_.adaptive_sample_step);
}
QualityState QualityController::state() const noexcept { return state_; }
} // namespace ldh
