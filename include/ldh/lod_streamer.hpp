#pragma once
#include "ldh/lod_scheduler.hpp"
#include "ldh/terrain_cache.hpp"
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
namespace ldh {
enum class StreamError { None, InvalidKey, InvalidField, InvalidSampleStep, InputTooLarge, QueueFull, OutOfRange, AlreadyQueued, AlreadyResident, CacheUnavailable, CacheMiss, CacheFailure, AllocationFailed };
struct LodStreamerConfig {
    std::size_t max_pending_tiles{8U};
    std::size_t max_resident_tiles{128U};
    std::size_t max_tile_samples{262144U};
};
struct ResidentTile {
    TerrainTileKey key;
    std::shared_ptr<const Mesh> mesh;
    std::uint64_t revision{};
    std::uint64_t last_used{};
};
struct LodStreamStats {
    std::size_t pending_tiles{};
    std::size_t resident_tiles{};
    std::size_t resident_vertices{};
    std::size_t resident_indices{};
    std::uint64_t completed_jobs{};
    std::uint64_t failed_jobs{};
    std::uint64_t cancelled_jobs{};
    std::uint64_t evicted_tiles{};
    std::uint64_t cache_hits{};
};
class LodStreamer final {
public:
    explicit LodStreamer(LodScheduler& scheduler, TerrainCache* cache = nullptr,
                         LodStreamerConfig config = {});
    ~LodStreamer();
    LodStreamer(const LodStreamer&) = delete;
    LodStreamer& operator=(const LodStreamer&) = delete;
    void set_focus(std::string world_id, std::int32_t center_x,
                   std::int32_t center_z, std::uint32_t radius) noexcept;
    [[nodiscard]] StreamError submit(TerrainTileKey key, HeightField field,
                                     std::uint32_t sample_step, std::int32_t priority,
                                     std::uint64_t revision = 0U) noexcept;
    // Poll consumes completed futures only; it never waits for worker completion.
    [[nodiscard]] std::size_t poll() noexcept;
    // Cache I/O is explicit. Run these methods away from the render thread.
    [[nodiscard]] StreamError restore_cached(const TerrainTileKey& key,
                                              std::uint64_t revision = 0U) noexcept;
    [[nodiscard]] CacheError persist(const TerrainTileKey& key) const noexcept;
    [[nodiscard]] std::vector<ResidentTile> resident_tiles() const;
    [[nodiscard]] LodStreamStats stats() const noexcept;
    void clear() noexcept;
private:
    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const TerrainTileKey& key) const noexcept;
    };
    struct Pending { TerrainTileKey key; std::uint64_t revision{}; std::future<LodJobResult> future; };
    [[nodiscard]] bool valid_key(const TerrainTileKey& key) const noexcept;
    [[nodiscard]] bool in_focus(const TerrainTileKey& key) const noexcept;
    void trim_resident_locked() noexcept;
    LodScheduler& scheduler_;
    TerrainCache* cache_{};
    LodStreamerConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, Pending> pending_;
    std::unordered_map<TerrainTileKey, std::uint64_t, KeyHash> pending_by_key_;
    std::unordered_map<TerrainTileKey, ResidentTile, KeyHash> resident_;
    std::string focus_world_;
    std::int32_t focus_x_{};
    std::int32_t focus_z_{};
    std::uint32_t focus_radius_{};
    bool focus_active_{};
    std::uint64_t clock_{};
    LodStreamStats totals_{};
};
} // namespace ldh
