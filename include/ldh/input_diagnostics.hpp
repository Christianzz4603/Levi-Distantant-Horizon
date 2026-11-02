#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace ldh {

struct InputDiagnosticsSnapshot {
    std::uint64_t events{};
    std::uint64_t downs{};
    std::uint64_t ups{};
    std::uint64_t moves{};
    std::uint64_t cancels{};
    std::uint64_t unmatchedUps{};
    std::uint64_t duplicateDowns{};
    std::uint64_t pointerOverflows{};
    std::uint64_t unknownActions{};
    std::uint32_t activePointers{};
    std::uint32_t possiblyStuckPointers{};
    std::uint64_t averagePressDurationMs{};
    std::uint64_t maximumPressDurationMs{};
};

/**
 * Passive Android MotionEvent telemetry for diagnosing lost or delayed touch
 * releases. This class never consumes, synthesizes, or changes game input.
 */
class InputDiagnostics final {
public:
    using Clock = std::chrono::steady_clock;

    void setEnabled(bool enabled) noexcept;
    [[nodiscard]] bool enabled() const noexcept;
    void setStaleThreshold(std::chrono::milliseconds threshold) noexcept;
    void observe(int action, int pointerId, Clock::time_point now = Clock::now()) noexcept;
    [[nodiscard]] InputDiagnosticsSnapshot snapshot(
        Clock::time_point now = Clock::now()) const noexcept;
    void reset() noexcept;

private:
    struct PointerState {
        int id{-1};
        Clock::time_point pressedAt{};
        bool active{};
    };

    static constexpr int kActionMask = 0xff;
    static constexpr int kActionDown = 0;
    static constexpr int kActionUp = 1;
    static constexpr int kActionMove = 2;
    static constexpr int kActionCancel = 3;
    static constexpr int kActionPointerDown = 5;
    static constexpr int kActionPointerUp = 6;

    mutable std::mutex mutex_;
    std::atomic_bool enabled_{false};
    std::array<PointerState, 10> pointers_{};
    std::chrono::milliseconds staleThreshold_{2500};
    std::uint64_t events_{};
    std::uint64_t downs_{};
    std::uint64_t ups_{};
    std::uint64_t moves_{};
    std::uint64_t cancels_{};
    std::uint64_t unmatchedUps_{};
    std::uint64_t duplicateDowns_{};
    std::uint64_t pointerOverflows_{};
    std::uint64_t unknownActions_{};
    std::uint64_t completedPresses_{};
    std::uint64_t totalPressDurationMs_{};
    std::uint64_t maximumPressDurationMs_{};
};

} // namespace ldh
