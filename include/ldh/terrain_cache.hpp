#pragma once
#include "ldh/lod_mesh.hpp"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
namespace ldh {
struct TerrainTileKey { std::string world_id; std::int32_t x{}; std::int32_t z{}; std::uint32_t lod{}; };
enum class CacheError { None, NotFound, InvalidKey, CorruptData, EntryTooLarge, IoError, AllocationFailed };
struct CacheResult { Mesh mesh; CacheError error{CacheError::None}; [[nodiscard]] explicit operator bool() const noexcept { return error == CacheError::None; } };
struct TerrainCacheConfig {
    std::filesystem::path directory;
    std::uint64_t max_bytes{64ULL * 1024ULL * 1024ULL};
    std::uint64_t max_entry_bytes{8ULL * 1024ULL * 1024ULL};
};
class TerrainCache final {
public:
    explicit TerrainCache(TerrainCacheConfig config);
    [[nodiscard]] CacheResult load(const TerrainTileKey& key) const;
    [[nodiscard]] CacheError store(const TerrainTileKey& key, const Mesh& mesh);
    [[nodiscard]] std::uint64_t bytes_used() const noexcept;
    [[nodiscard]] bool prune() noexcept;
private:
    [[nodiscard]] std::filesystem::path path_for(const TerrainTileKey& key) const;
    [[nodiscard]] bool valid_key(const TerrainTileKey& key) const noexcept;
    TerrainCacheConfig config_;
    mutable std::mutex mutex_;
};
} // namespace ldh
