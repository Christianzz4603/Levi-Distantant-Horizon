#pragma once
#include "ldh/lod_mesh.hpp"
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
namespace ldh {
struct LodSchedulerConfig { std::size_t worker_count{2U}; std::size_t max_pending{8U}; std::size_t max_input_samples{262144U}; };
struct LodJob { std::uint64_t id{}; std::int32_t priority{}; HeightField field; std::uint32_t sample_step{1U}; };
enum class SchedulerError { None, Stopped, QueueFull, DuplicateJobId, InputTooLarge, Cancelled, AllocationFailed };
struct LodJobResult { std::uint64_t id{}; MeshResult mesh_result; SchedulerError scheduler_error{SchedulerError::None}; };
class LodScheduler final {
public:
    explicit LodScheduler(LodSchedulerConfig config = {});
    ~LodScheduler();
    LodScheduler(const LodScheduler&) = delete;
    LodScheduler& operator=(const LodScheduler&) = delete;
    LodScheduler(LodScheduler&&) = delete;
    LodScheduler& operator=(LodScheduler&&) = delete;
    [[nodiscard]] std::future<LodJobResult> submit(LodJob job);
    [[nodiscard]] bool cancel(std::uint64_t job_id) noexcept;
    [[nodiscard]] std::size_t pending_count() const noexcept;
    [[nodiscard]] std::size_t worker_count() const noexcept;
private:
    struct JobState { LodJob job; std::shared_ptr<std::atomic_bool> cancelled; std::promise<LodJobResult> promise; std::uint64_t sequence{}; };
    struct LowerPriority { bool operator()(const std::shared_ptr<JobState>& a, const std::shared_ptr<JobState>& b) const noexcept; };
    void worker_loop() noexcept;
    void shutdown() noexcept;
    LodSchedulerConfig config_;
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::priority_queue<std::shared_ptr<JobState>, std::vector<std::shared_ptr<JobState>>, LowerPriority> queue_;
    std::unordered_map<std::uint64_t, std::weak_ptr<JobState>> jobs_;
    std::vector<std::thread> workers_;
    std::uint64_t next_sequence_{};
    bool stopping_{};
};
} // namespace ldh
