#include "ldh/performance_governor.hpp"

#include <cassert>
#include <cstddef>

int main() {
    using namespace ldh;

    PerformanceGovernor low_end(
        PerformanceGovernor::config_for(PerformanceProfile::LowEnd));
    auto low_budget = low_end.budget();
    assert(low_budget.sample_step == 4U);
    assert(low_budget.jobs_per_update == 1U);
    assert(low_budget.lod_radius_chunks == 24U);
    assert(low_budget.cache_budget_bytes == 8U * 1024U * 1024U);

    auto config = PerformanceGovernor::config_for(PerformanceProfile::Balanced);
    config.degrade_after_frames = 2U;
    config.recover_after_frames = 2U;
    config.ema_alpha = 1.0;
    PerformanceGovernor governor(config);

    const auto initial_step = governor.budget().sample_step;
    governor.observe_frame(60.0);
    governor.observe_frame(60.0);
    const auto overloaded = governor.budget();
    assert(overloaded.sample_step > initial_step);
    assert(overloaded.jobs_per_update <= 2U);
    assert(overloaded.lod_radius_chunks < 48U);

    governor.observe_frame(5.0);
    governor.observe_frame(5.0);
    assert(governor.budget().sample_step == initial_step);

    governor.observe_memory_usage(95U, 100U);
    const auto critical = governor.budget();
    assert(critical.skip_optional_work);
    assert(critical.jobs_per_update == 1U);
    assert(critical.lod_radius_chunks == 8U);
    assert(critical.cache_budget_bytes == 4U * 1024U * 1024U);

    governor.observe_memory_usage(10U, 100U);
    assert(!governor.budget().skip_optional_work);

    governor.set_profile(PerformanceProfile::Quality);
    const auto quality = governor.budget();
    assert(quality.sample_step == 1U);
    assert(quality.lod_radius_chunks == 96U);
    assert(quality.jobs_per_update == 4U);

    governor.observe_frame(0.0);
    governor.observe_frame(-1.0);
    assert(governor.state().observed_frames == 4U);

    return 0;
}
