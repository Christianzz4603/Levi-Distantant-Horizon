#include "ldh/performance_governor.hpp"

#include <algorithm>
#include <cmath>

namespace ldh {
namespace {
constexpr std::size_t kMiB = 1024U * 1024U;

std::size_t scaled_down(std::size_t value, std::size_t divisor) noexcept {
    return std::max<std::size_t>(1U, value / std::max<std::size_t>(1U, divisor));
}
} // namespace

PerformanceGovernorConfig PerformanceGovernor::config_for(
    PerformanceProfile profile) noexcept {
    PerformanceGovernorConfig config{};
    config.profile = profile;

    switch (profile) {
    case PerformanceProfile::LowEnd:
        config.target_frame_ms = 33.33;
        config.base_sample_step = 4U;
        config.max_sample_step = 16U;
        config.base_jobs_per_update = 1U;
        config.base_lod_radius_chunks = 24U;
        config.base_cache_budget_bytes = 8U * kMiB;
        config.base_max_pending_jobs = 2U;
        config.max_input_samples = 65536U;
        break;
    case PerformanceProfile::Quality:
        config.target_frame_ms = 16.67;
        config.base_sample_step = 1U;
        config.max_sample_step = 8U;
        config.base_jobs_per_update = 4U;
        config.base_lod_radius_chunks = 96U;
        config.base_cache_budget_bytes = 48U * kMiB;
        config.base_max_pending_jobs = 8U;
        config.max_input_samples = 262144U;
        break;
    case PerformanceProfile::Balanced:
    default:
        config.target_frame_ms = 33.33;
        config.base_sample_step = 2U;
        config.max_sample_step = 16U;
        config.base_jobs_per_update = 2U;
        config.base_lod_radius_chunks = 48U;
        config.base_cache_budget_bytes = 16U * kMiB;
        config.base_max_pending_jobs = 4U;
        config.max_input_samples = 131072U;
        break;
    }
    return config;
}

PerformanceGovernor::PerformanceGovernor(PerformanceGovernorConfig config)
    : config_(config) {
    normalize();
    state_.adaptive_sample_step = config_.base_sample_step;
}

void PerformanceGovernor::normalize() noexcept {
    if (!std::isfinite(config_.target_frame_ms) ||
        config_.target_frame_ms <= 0.0) {
        config_.target_frame_ms = 33.33;
    }
    if (!std::isfinite(config_.ema_alpha)) {
        config_.ema_alpha = 0.15;
    }
    config_.ema_alpha = std::clamp(config_.ema_alpha, 0.01, 1.0);
    if (!std::isfinite(config_.degrade_ratio) ||
        config_.degrade_ratio <= 1.0) {
        config_.degrade_ratio = 1.15;
    }
    if (!std::isfinite(config_.recover_ratio) ||
        config_.recover_ratio <= 0.0 || config_.recover_ratio >= 1.0) {
        config_.recover_ratio = 0.78;
    }

    config_.degrade_after_frames = std::max(1U, config_.degrade_after_frames);
    config_.recover_after_frames = std::max(1U, config_.recover_after_frames);
    config_.base_sample_step = std::max(1U, config_.base_sample_step);
    config_.max_sample_step =
        std::max(config_.base_sample_step, config_.max_sample_step);
    config_.base_jobs_per_update =
        std::max<std::size_t>(1U, config_.base_jobs_per_update);
    config_.base_lod_radius_chunks =
        std::max<std::size_t>(8U, config_.base_lod_radius_chunks);
    config_.base_cache_budget_bytes =
        std::max<std::size_t>(4U * kMiB, config_.base_cache_budget_bytes);
    config_.base_max_pending_jobs =
        std::max<std::size_t>(1U, config_.base_max_pending_jobs);
    config_.max_input_samples =
        std::max<std::size_t>(1U, config_.max_input_samples);
}

void PerformanceGovernor::set_profile(PerformanceProfile profile) noexcept {
    config_ = config_for(profile);
    normalize();
    state_.adaptive_sample_step = config_.base_sample_step;
    state_.overloaded_frames = 0U;
    state_.headroom_frames = 0U;
}

void PerformanceGovernor::observe_frame(double frame_ms) noexcept {
    if (!std::isfinite(frame_ms) || frame_ms <= 0.0) {
        return;
    }

    if (!has_frame_average_) {
        state_.average_frame_ms = frame_ms;
        has_frame_average_ = true;
    } else {
        state_.average_frame_ms +=
            config_.ema_alpha * (frame_ms - state_.average_frame_ms);
    }
    ++state_.observed_frames;

    if (state_.average_frame_ms >
        config_.target_frame_ms * config_.degrade_ratio) {
        state_.headroom_frames = 0U;
        if (state_.overloaded_frames < config_.degrade_after_frames) {
            ++state_.overloaded_frames;
        }
        if (state_.overloaded_frames >= config_.degrade_after_frames) {
            const auto doubled =
                state_.adaptive_sample_step > config_.max_sample_step / 2U
                    ? config_.max_sample_step
                    : state_.adaptive_sample_step * 2U;
            state_.adaptive_sample_step =
                std::min(config_.max_sample_step, doubled);
            state_.overloaded_frames = 0U;
        }
    } else if (state_.average_frame_ms <
               config_.target_frame_ms * config_.recover_ratio) {
        state_.overloaded_frames = 0U;
        if (state_.headroom_frames < config_.recover_after_frames) {
            ++state_.headroom_frames;
        }
        if (state_.headroom_frames >= config_.recover_after_frames) {
            state_.adaptive_sample_step = std::max(
                config_.base_sample_step, state_.adaptive_sample_step / 2U);
            state_.headroom_frames = 0U;
        }
    } else {
        state_.overloaded_frames = 0U;
        state_.headroom_frames = 0U;
    }
}

void PerformanceGovernor::observe_memory_usage(
    std::size_t used_bytes,
    std::size_t available_budget_bytes) noexcept {
    if (available_budget_bytes == 0U) {
        return;
    }

    const auto bounded_used = std::min(used_bytes, available_budget_bytes);
    state_.memory_pressure =
        static_cast<double>(bounded_used) /
        static_cast<double>(available_budget_bytes);
}

PerformanceBudget PerformanceGovernor::budget() const noexcept {
    PerformanceBudget result{};
    const auto step = std::clamp(state_.adaptive_sample_step,
                                 config_.base_sample_step,
                                 config_.max_sample_step);
    const auto scale = std::max<std::uint32_t>(
        1U, step / config_.base_sample_step);

    result.sample_step = step;
    result.jobs_per_update =
        scaled_down(config_.base_jobs_per_update, scale);
    result.lod_radius_chunks =
        std::max<std::size_t>(8U, config_.base_lod_radius_chunks / scale);
    result.cache_budget_bytes = config_.base_cache_budget_bytes;
    result.max_pending_jobs =
        scaled_down(config_.base_max_pending_jobs, scale);
    result.max_input_samples = config_.max_input_samples;

    if (state_.memory_pressure >= 0.85) {
        result.jobs_per_update = 1U;
        result.lod_radius_chunks =
            std::max<std::size_t>(8U, result.lod_radius_chunks * 3U / 4U);
        result.cache_budget_bytes =
            std::max<std::size_t>(4U * kMiB,
                                  result.cache_budget_bytes * 3U / 4U);
        result.max_pending_jobs = 1U;
        result.max_input_samples =
            std::max<std::size_t>(16384U, result.max_input_samples / 2U);
    }

    if (state_.memory_pressure >= 0.95) {
        result.sample_step = config_.max_sample_step;
        result.lod_radius_chunks = 8U;
        result.cache_budget_bytes = 4U * kMiB;
        result.max_input_samples = 16384U;
        result.skip_optional_work = true;
    }

    return result;
}

PerformanceState PerformanceGovernor::state() const noexcept {
    return state_;
}

} // namespace ldh
