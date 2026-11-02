#pragma once
#include "ldh/terrain_cache.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
namespace ldh {
enum class MemoryChunkError { None, InvalidKey, InvalidField, EntryTooLarge, CapacityTooSmall, AllocationFailed };
struct MemoryChunkCacheConfig {
    std::size_t max_bytes{16U * 1024U * 1024U};
    std::size_t max_entry_bytes{2U * 1024U * 1024U};
    std::size_t max_entries{256U};
};
struct MemoryChunkCacheStats {
    std::size_t entries{};
    std::size_t bytes_used{};
    std::size_t capacity_bytes{};
    std::uint64_t hits{};
    std::uint64_t misses{};
    std::uint64_t evictions{};
};
class MemoryChunkCache final {
public:
    explicit MemoryChunkCache(MemoryChunkCacheConfig config = {});
    [[nodiscard]] MemoryChunkError put(const TerrainTileKey& key, HeightField field) noexcept;
    [[nodiscard]] std::optional<HeightField> get(const TerrainTileKey& key) noexcept;
    [[nodiscard]] bool erase(const TerrainTileKey& key) noexcept;
    void clear() noexcept;
    [[nodiscard]] MemoryChunkCacheStats stats() const noexcept;
private:
    struct KeyHash {
        [[nodiscard]] std::size_t operator()(const TerrainTileKey& key) const noexcept;
    };
    struct Entry {
        HeightField field;
        std::size_t bytes{};
        std::list<TerrainTileKey>::iterator lru;
    };
    [[nodiscard]] bool valid_key(const TerrainTileKey& key) const noexcept;
    [[nodiscard]] static bool valid_field(const HeightField& field, std::size_t& bytes) noexcept;
    void trim_locked() noexcept;
    MemoryChunkCacheConfig config_;
    mutable std::mutex mutex_;
    std::unordered_map<TerrainTileKey, Entry, KeyHash> entries_;
    std::list<TerrainTileKey> lru_;
    std::size_t bytes_used_{};
    std::uint64_t hits_{};
    std::uint64_t misses_{};
    std::uint64_t evictions_{};
};
} // namespace ldh
