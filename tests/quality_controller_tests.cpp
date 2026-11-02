#include "ldh/quality_controller.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
namespace {
void degrades_under_pressure_and_recovers_slowly() {
    ldh::QualityController controller({16.0, 1.0, 1.10, 0.75, 3U, 5U, 16U, 4U});
    for (int i = 0; i < 3; ++i) controller.observe_frame(30.0);
    auto state = controller.state();
    assert(state.adaptive_sample_step == 2U);
    assert(controller.sample_step_for_distance(20.0) == 2U);
    assert(controller.sample_step_for_distance(300.0) == 16U);
    assert(controller.recommended_jobs_per_update() == 2U);
    for (int i = 0; i < 5; ++i) controller.observe_frame(5.0);
    state = controller.state();
    assert(state.adaptive_sample_step == 1U);
    assert(controller.recommended_jobs_per_update() == 4U);
}
void ignores_invalid_samples_and_uses_distance_bands() {
    ldh::QualityController controller;
    controller.observe_frame(0.0); controller.observe_frame(-1.0); controller.observe_frame(std::nan(""));
    assert(controller.state().observed_frames == 0U);
    assert(controller.sample_step_for_distance(32.0) == 1U);
    assert(controller.sample_step_for_distance(65.0) == 2U);
    assert(controller.sample_step_for_distance(129.0) == 4U);
    assert(controller.sample_step_for_distance(257.0) == 8U);
    assert(controller.sample_step_for_distance(-5.0) == 16U);
}
}
int main() {
    degrades_under_pressure_and_recovers_slowly(); ignores_invalid_samples_and_uses_distance_bands();
    std::cout << "All quality controller tests passed.\n";
}
