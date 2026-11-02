#include "ldh/input_diagnostics.hpp"

#include <cassert>
#include <chrono>

int main() {
    using Clock = ldh::InputDiagnostics::Clock;
    using namespace std::chrono_literals;

    ldh::InputDiagnostics monitor;
    const auto start = Clock::time_point{};
    monitor.observe(0, 1, start);
    assert(monitor.snapshot(start).events == 0);

    monitor.setEnabled(true);
    monitor.setStaleThreshold(100ms); // Clamped to the supported minimum.
    monitor.observe(0, 1, start);
    monitor.observe(5 | (1 << 8), 2, start + 10ms);
    auto active = monitor.snapshot(start + 300ms);
    assert(active.events == 2);
    assert(active.downs == 2);
    assert(active.activePointers == 2);
    assert(active.possiblyStuckPointers == 2);

    monitor.observe(1, 1, start + 40ms);
    monitor.observe(6 | (1 << 8), 2, start + 60ms);
    auto released = monitor.snapshot(start + 70ms);
    assert(released.activePointers == 0);
    assert(released.ups == 2);
    assert(released.averagePressDurationMs == 45);
    assert(released.maximumPressDurationMs == 50);

    monitor.observe(0, 3, start + 80ms);
    monitor.observe(3, 3, start + 90ms);
    assert(monitor.snapshot(start + 100ms).activePointers == 0);

    monitor.observe(1, 99, start + 110ms);
    monitor.observe(0, 4, start + 120ms);
    monitor.observe(0, 4, start + 130ms);
    const auto edgeCases = monitor.snapshot(start + 140ms);
    assert(edgeCases.unmatchedUps == 1);
    assert(edgeCases.duplicateDowns == 1);
    assert(edgeCases.cancels == 1);

    monitor.setEnabled(false);
    monitor.observe(0, 5, start + 150ms);
    assert(monitor.snapshot(start + 160ms).events == edgeCases.events);
    return 0;
}
