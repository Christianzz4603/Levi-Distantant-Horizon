#include "ldh/lod_scheduler.hpp"
#include <algorithm>
#include <new>
#include <utility>
namespace ldh { namespace {
std::future<LodJobResult> ready(std::uint64_t id, SchedulerError error) {
    std::promise<LodJobResult> p; auto f = p.get_future(); LodJobResult r{}; r.id = id; r.scheduler_error = error; p.set_value(std::move(r)); return f;
}
}
bool LodScheduler::LowerPriority::operator()(const std::shared_ptr<JobState>& a, const std::shared_ptr<JobState>& b) const noexcept {
    if (a->job.priority != b->job.priority) return a->job.priority < b->job.priority;
    return a->sequence > b->sequence;
}
LodScheduler::LodScheduler(LodSchedulerConfig config) : config_(config) {
    config_.worker_count = std::clamp(config_.worker_count, std::size_t{1}, std::size_t{8});
    config_.max_pending = std::max(config_.max_pending, std::size_t{1});
    config_.max_input_samples = std::max(config_.max_input_samples, std::size_t{1});
    workers_.reserve(config_.worker_count);
    try { for (std::size_t i = 0; i < config_.worker_count; ++i) workers_.emplace_back([this] { worker_loop(); }); }
    catch (...) { shutdown(); throw; }
}
LodScheduler::~LodScheduler() { shutdown(); }
std::future<LodJobResult> LodScheduler::submit(LodJob job) {
    const auto id = job.id; const std::size_t w = job.field.width, d = job.field.depth;
    if (d != 0U && w > config_.max_input_samples / d) return ready(id, SchedulerError::InputTooLarge);
    if (w * d > config_.max_input_samples) return ready(id, SchedulerError::InputTooLarge);
    std::shared_ptr<JobState> state;
    try { state = std::make_shared<JobState>(); state->job = std::move(job); state->cancelled = std::make_shared<std::atomic_bool>(false); }
    catch (const std::bad_alloc&) { return ready(id, SchedulerError::AllocationFailed); }
    auto future = state->promise.get_future();
    {
        std::lock_guard lock(mutex_);
        if (stopping_) { state->promise.set_value({id, {}, SchedulerError::Stopped}); return future; }
        if (jobs_.contains(id)) { state->promise.set_value({id, {}, SchedulerError::DuplicateJobId}); return future; }
        if (queue_.size() >= config_.max_pending) { state->promise.set_value({id, {}, SchedulerError::QueueFull}); return future; }
        state->sequence = next_sequence_++;
        try { jobs_.emplace(id, state); queue_.push(state); }
        catch (const std::bad_alloc&) { jobs_.erase(id); state->promise.set_value({id, {}, SchedulerError::AllocationFailed}); return future; }
    }
    condition_.notify_one(); return future;
}
bool LodScheduler::cancel(std::uint64_t id) noexcept {
    std::lock_guard lock(mutex_); const auto it = jobs_.find(id); if (it == jobs_.end()) return false;
    const auto state = it->second.lock(); if (!state) { jobs_.erase(it); return false; }
    state->cancelled->store(true, std::memory_order_relaxed); return true;
}
std::size_t LodScheduler::pending_count() const noexcept { std::lock_guard lock(mutex_); return queue_.size(); }
std::size_t LodScheduler::worker_count() const noexcept { return workers_.size(); }
void LodScheduler::worker_loop() noexcept {
    for (;;) {
        std::shared_ptr<JobState> state;
        { std::unique_lock lock(mutex_); condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
          if (stopping_ && queue_.empty()) return; state = queue_.top(); queue_.pop(); }
        LodJobResult result{}; result.id = state->job.id;
        if (state->cancelled->load(std::memory_order_relaxed)) result.scheduler_error = SchedulerError::Cancelled;
        else {
            try { result.mesh_result = build_lod_mesh(state->job.field, state->job.sample_step, state->cancelled.get());
                  if (result.mesh_result.error == MeshError::Cancelled) result.scheduler_error = SchedulerError::Cancelled; }
            catch (const std::bad_alloc&) { result.scheduler_error = SchedulerError::AllocationFailed; }
            catch (...) { result.scheduler_error = SchedulerError::AllocationFailed; }
        }
        try { state->promise.set_value(std::move(result)); } catch (...) {}
        { std::lock_guard lock(mutex_); jobs_.erase(state->job.id); }
    }
}
void LodScheduler::shutdown() noexcept {
    { std::lock_guard lock(mutex_); stopping_ = true;
      for (auto& entry : jobs_) if (const auto state = entry.second.lock()) state->cancelled->store(true, std::memory_order_relaxed);
      while (!queue_.empty()) { auto state = queue_.top(); queue_.pop();
          try { state->promise.set_value({state->job.id, {}, SchedulerError::Cancelled}); } catch (...) {}
          jobs_.erase(state->job.id);
      }
    }
    condition_.notify_all();
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
    workers_.clear();
}
} // namespace ldh
