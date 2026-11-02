#include "ldh/input_diagnostics.hpp"

#include <algorithm>

namespace ldh {

void InputDiagnostics::setEnabled(bool enabled) noexcept {
    enabled_.store(enabled, std::memory_order_release);
}

bool InputDiagnostics::enabled() const noexcept {
    return enabled_.load(std::memory_order_acquire);
}

void InputDiagnostics::setStaleThreshold(std::chrono::milliseconds threshold) noexcept {
    const auto bounded = std::clamp<std::int64_t>(threshold.count(), 250, 10000);
    std::lock_guard lock(mutex_);
    staleThreshold_ = std::chrono::milliseconds(bounded);
}

void InputDiagnostics::observe(int action, int pointerId, Clock::time_point now) noexcept {
    if (!enabled()) return;

    const int maskedAction = action & kActionMask;
    std::lock_guard lock(mutex_);
    ++events_;

    if (maskedAction == kActionDown || maskedAction == kActionPointerDown) {
        ++downs_;
        PointerState *slot = nullptr;
        for (auto &pointer : pointers_) {
            if (pointer.active && pointer.id == pointerId) {
                ++duplicateDowns_;
                slot = &pointer;
                break;
            }
            if (!pointer.active && slot == nullptr) slot = &pointer;
        }
        if (slot == nullptr) {
            ++pointerOverflows_;
            return;
        }
        slot->id = pointerId;
        slot->pressedAt = now;
        slot->active = true;
        return;
    }

    if (maskedAction == kActionUp || maskedAction == kActionPointerUp) {
        ++ups_;
        for (auto &pointer : pointers_) {
            if (!pointer.active || pointer.id != pointerId) continue;
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - pointer.pressedAt).count();
            const auto duration = static_cast<std::uint64_t>(std::max<std::int64_t>(0, elapsed));
            ++completedPresses_;
            totalPressDurationMs_ += duration;
            maximumPressDurationMs_ = std::max(maximumPressDurationMs_, duration);
            pointer.active = false;
            pointer.id = -1;
            return;
        }
        ++unmatchedUps_;
        return;
    }

    if (maskedAction == kActionMove) {
        ++moves_;
        return;
    }

    if (maskedAction == kActionCancel) {
        ++cancels_;
        for (auto &pointer : pointers_) {
            pointer.active = false;
            pointer.id = -1;
        }
        return;
    }

    ++unknownActions_;
}

InputDiagnosticsSnapshot InputDiagnostics::snapshot(Clock::time_point now) const noexcept {
    std::lock_guard lock(mutex_);
    InputDiagnosticsSnapshot result{};
    result.events = events_;
    result.downs = downs_;
    result.ups = ups_;
    result.moves = moves_;
    result.cancels = cancels_;
    result.unmatchedUps = unmatchedUps_;
    result.duplicateDowns = duplicateDowns_;
    result.pointerOverflows = pointerOverflows_;
    result.unknownActions = unknownActions_;
    result.maximumPressDurationMs = maximumPressDurationMs_;
    if (completedPresses_ != 0) {
        result.averagePressDurationMs = totalPressDurationMs_ / completedPresses_;
    }
    for (const auto &pointer : pointers_) {
        if (!pointer.active) continue;
        ++result.activePointers;
        if (now - pointer.pressedAt >= staleThreshold_) ++result.possiblyStuckPointers;
    }
    return result;
}

void InputDiagnostics::reset() noexcept {
    std::lock_guard lock(mutex_);
    for (auto &pointer : pointers_) {
        pointer = {};
        pointer.id = -1;
    }
    events_ = downs_ = ups_ = moves_ = cancels_ = 0;
    unmatchedUps_ = duplicateDowns_ = pointerOverflows_ = unknownActions_ = 0;
    completedPresses_ = totalPressDurationMs_ = maximumPressDurationMs_ = 0;
}

} // namespace ldh
