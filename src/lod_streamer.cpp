#include "ldh/lod_streamer.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <new>
#include <utility>
namespace ldh {
namespace {
std::atomic<std::uint64_t> next_job_id{1U};
bool valid_field_shape(const HeightField& field, std::size_t max_samples,
                       bool& too_large) noexcept {
    too_large = false;
    if (field.width < 2U || field.depth < 2U ||
        !std::isfinite(field.cell_size) || field.cell_size <= 0.0F) return false;
    const auto width = static_cast<std::size_t>(field.width);
    const auto depth = static_cast<std::size_t>(field.depth);
    if (width > std::numeric_limits<std::size_t>::max() / depth) return false;
    const auto count = width * depth;
    if (count != field.heights.size()) return false;
    too_large = count > max_samples;
    return !too_large;
}
}
std::size_t LodStreamer::KeyHash::operator()(const TerrainTileKey& key) const noexcept {
    std::size_t hash = std::hash<std::string>{}(key.world_id);
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= static_cast<std::size_t>(value) + static_cast<std::size_t>(0x9e3779b9U) +
                (hash << 6U) + (hash >> 2U);
    };
    mix(static_cast<std::uint32_t>(key.x));
    mix(static_cast<std::uint32_t>(key.z));
    mix(key.lod);
    return hash;
}
LodStreamer::LodStreamer(LodScheduler& scheduler, TerrainCache* cache,
                         LodStreamerConfig config)
    : scheduler_(scheduler), cache_(cache), config_(config) {
    config_.max_pending_tiles = std::clamp<std::size_t>(config_.max_pending_tiles, 1U, 64U);
    config_.max_resident_tiles = std::clamp<std::size_t>(config_.max_resident_tiles, 1U, 1024U);
    config_.max_tile_samples = std::clamp<std::size_t>(config_.max_tile_samples, 4U, 1048576U);
}
LodStreamer::~LodStreamer() { clear(); }
bool LodStreamer::valid_key(const TerrainTileKey& key) const noexcept {
    return !key.world_id.empty() && key.world_id.size() <= 128U;
}
bool LodStreamer::in_focus(const TerrainTileKey& key) const noexcept {
    if (!focus_active_) return true;
    if (key.world_id != focus_world_) return false;
    const auto dx = static_cast<std::int64_t>(key.x) - static_cast<std::int64_t>(focus_x_);
    const auto dz = static_cast<std::int64_t>(key.z) - static_cast<std::int64_t>(focus_z_);
    const auto radius = static_cast<std::int64_t>(focus_radius_);
    return dx >= -radius && dx <= radius && dz >= -radius && dz <= radius;
}
void LodStreamer::set_focus(std::string world_id, std::int32_t center_x,
                            std::int32_t center_z, std::uint32_t radius) noexcept {
    try {
        std::lock_guard lock(mutex_);
        focus_world_ = std::move(world_id);
        focus_x_ = center_x; focus_z_ = center_z; focus_radius_ = radius;
        focus_active_ = !focus_world_.empty();
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (!in_focus(it->second.key)) {
                (void)scheduler_.cancel(it->first);
                pending_by_key_.erase(it->second.key);
                it = pending_.erase(it);
                ++totals_.cancelled_jobs;
            } else ++it;
        }
        for (auto it = resident_.begin(); it != resident_.end();) {
            if (!in_focus(it->first)) { it = resident_.erase(it); ++totals_.evicted_tiles; }
            else ++it;
        }
    } catch (...) {}
}
StreamError LodStreamer::submit(TerrainTileKey key, HeightField field,
                                std::uint32_t sample_step, std::int32_t priority,
                                std::uint64_t revision) noexcept {
    if (!valid_key(key)) return StreamError::InvalidKey;
    if (sample_step == 0U) return StreamError::InvalidSampleStep;
    bool too_large = false;
    if (!valid_field_shape(field, config_.max_tile_samples, too_large))
        return too_large ? StreamError::InputTooLarge : StreamError::InvalidField;
    try {
        std::lock_guard lock(mutex_);
        if (!in_focus(key)) return StreamError::OutOfRange;
        if (const auto found = resident_.find(key); found != resident_.end()) {
            if (found->second.revision >= revision) {
                found->second.last_used = ++clock_;
                return StreamError::AlreadyResident;
            }
            resident_.erase(found);
        }
        if (const auto found = pending_by_key_.find(key); found != pending_by_key_.end()) {
            const auto pending = pending_.find(found->second);
            if (pending != pending_.end()) {
                if (pending->second.revision >= revision) return StreamError::AlreadyQueued;
                (void)scheduler_.cancel(found->second);
                pending_.erase(pending);
            }
            pending_by_key_.erase(found);
        }
        if (pending_.size() >= config_.max_pending_tiles) return StreamError::QueueFull;
        auto id = next_job_id.fetch_add(1U, std::memory_order_relaxed);
        if (id == 0U) id = next_job_id.fetch_add(1U, std::memory_order_relaxed);
        LodJob job{};
        job.id = id; job.priority = priority; job.field = std::move(field);
        job.sample_step = sample_step;
        auto future = scheduler_.submit(std::move(job));
        try {
            pending_.emplace(id, Pending{key, revision, std::move(future)});
            pending_by_key_.emplace(std::move(key), id);
        } catch (...) {
            (void)scheduler_.cancel(id);
            pending_.erase(id);
            return StreamError::AllocationFailed;
        }
        totals_.pending_tiles = pending_.size();
        return StreamError::None;
    } catch (const std::bad_alloc&) { return StreamError::AllocationFailed; }
      catch (...) { return StreamError::QueueFull; }
}
std::size_t LodStreamer::poll() noexcept {
    std::size_t completed = 0U;
    try {
        std::lock_guard lock(mutex_);
        for (auto it = pending_.begin(); it != pending_.end();) {
            if (it->second.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                ++it; continue;
            }
            LodJobResult result{};
            bool received = false;
            try { result = it->second.future.get(); received = true; }
            catch (...) { ++totals_.failed_jobs; }
            const auto key = it->second.key;
            const auto revision = it->second.revision;
            pending_by_key_.erase(key);
            it = pending_.erase(it);
            ++completed;
            if (!received || result.scheduler_error != SchedulerError::None || !result.mesh_result) {
                if (received && result.scheduler_error == SchedulerError::Cancelled)
                    ++totals_.cancelled_jobs;
                else ++totals_.failed_jobs;
                continue;
            }
            if (!in_focus(key)) continue;
            try {
                auto mesh = std::make_shared<Mesh>(std::move(result.mesh_result.mesh));
                resident_[key] = ResidentTile{key, std::move(mesh), revision, ++clock_};
                ++totals_.completed_jobs;
                trim_resident_locked();
            } catch (...) { ++totals_.failed_jobs; }
        }
        totals_.pending_tiles = pending_.size();
        totals_.resident_tiles = resident_.size();
    } catch (...) {}
    return completed;
}
void LodStreamer::trim_resident_locked() noexcept {
    while (resident_.size() > config_.max_resident_tiles) {
        auto oldest = resident_.end();
        for (auto it = resident_.begin(); it != resident_.end(); ++it)
            if (oldest == resident_.end() || it->second.last_used < oldest->second.last_used)
                oldest = it;
        if (oldest == resident_.end()) break;
        resident_.erase(oldest); ++totals_.evicted_tiles;
    }
}
StreamError LodStreamer::restore_cached(const TerrainTileKey& key,
                                        std::uint64_t revision) noexcept {
    if (!valid_key(key)) return StreamError::InvalidKey;
    if (!cache_) return StreamError::CacheUnavailable;
    try {
        {
            std::lock_guard lock(mutex_);
            if (!in_focus(key)) return StreamError::OutOfRange;
            if (resident_.contains(key)) return StreamError::AlreadyResident;
        }
        auto loaded = cache_->load(key);
        if (!loaded) return loaded.error == CacheError::NotFound ? StreamError::CacheMiss
                                                                 : StreamError::CacheFailure;
        auto mesh = std::make_shared<Mesh>(std::move(loaded.mesh));
        std::lock_guard lock(mutex_);
        if (!in_focus(key)) return StreamError::OutOfRange;
        resident_[key] = ResidentTile{key, std::move(mesh), revision, ++clock_};
        ++totals_.cache_hits;
        trim_resident_locked();
        totals_.resident_tiles = resident_.size();
        return StreamError::None;
    } catch (...) { return StreamError::AllocationFailed; }
}
CacheError LodStreamer::persist(const TerrainTileKey& key) const noexcept {
    if (!cache_) return CacheError::IoError;
    std::shared_ptr<const Mesh> mesh;
    {
        std::lock_guard lock(mutex_);
        const auto found = resident_.find(key);
        if (found == resident_.end() || !found->second.mesh) return CacheError::NotFound;
        mesh = found->second.mesh;
    }
    return cache_->store(key, *mesh);
}
std::vector<ResidentTile> LodStreamer::resident_tiles() const {
    std::lock_guard lock(mutex_);
    std::vector<ResidentTile> result;
    result.reserve(resident_.size());
    for (const auto& entry : resident_) result.push_back(entry.second);
    return result;
}
LodStreamStats LodStreamer::stats() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        auto result = totals_;
        result.pending_tiles = pending_.size();
        result.resident_tiles = resident_.size();
        result.resident_vertices = 0U; result.resident_indices = 0U;
        for (const auto& entry : resident_) if (entry.second.mesh) {
            result.resident_vertices += entry.second.mesh->vertices.size();
            result.resident_indices += entry.second.mesh->indices.size();
        }
        return result;
    } catch (...) { return {}; }
}
void LodStreamer::clear() noexcept {
    try {
        std::lock_guard lock(mutex_);
        for (const auto& entry : pending_) (void)scheduler_.cancel(entry.first);
        pending_.clear(); pending_by_key_.clear(); resident_.clear();
        totals_.pending_tiles = 0U; totals_.resident_tiles = 0U;
    } catch (...) {}
}
} // namespace ldh
