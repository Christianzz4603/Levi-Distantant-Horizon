#include "ldh/memory_chunk_cache.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <new>
#include <utility>
namespace ldh {
MemoryChunkCache::MemoryChunkCache(MemoryChunkCacheConfig config) : config_(config) {
    config_.max_bytes = std::clamp<std::size_t>(config_.max_bytes, 1024U, 256U * 1024U * 1024U);
    config_.max_entry_bytes = std::clamp<std::size_t>(config_.max_entry_bytes, 16U, config_.max_bytes);
    config_.max_entries = std::clamp<std::size_t>(config_.max_entries, 1U, 4096U);
}
std::size_t MemoryChunkCache::KeyHash::operator()(const TerrainTileKey& key) const noexcept {
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
bool MemoryChunkCache::valid_key(const TerrainTileKey& key) const noexcept {
    return !key.world_id.empty() && key.world_id.size() <= 128U;
}
bool MemoryChunkCache::valid_field(const HeightField& field, std::size_t& bytes) noexcept {
    bytes = 0U;
    if (field.width < 2U || field.depth < 2U || !std::isfinite(field.cell_size) ||
        field.cell_size <= 0.0F) return false;
    const auto width = static_cast<std::size_t>(field.width);
    const auto depth = static_cast<std::size_t>(field.depth);
    if (width > std::numeric_limits<std::size_t>::max() / depth) return false;
    const auto count = width * depth;
    if (count != field.heights.size() ||
        count > (std::numeric_limits<std::size_t>::max() - sizeof(HeightField)) / sizeof(float))
        return false;
    for (const float height : field.heights) if (!std::isfinite(height)) return false;
    bytes = sizeof(HeightField) + count * sizeof(float);
    return true;
}
MemoryChunkError MemoryChunkCache::put(const TerrainTileKey& key, HeightField field) noexcept {
    if (!valid_key(key)) return MemoryChunkError::InvalidKey;
    std::size_t bytes = 0U;
    if (!valid_field(field, bytes)) return MemoryChunkError::InvalidField;
    if (bytes > config_.max_entry_bytes) return MemoryChunkError::EntryTooLarge;
    if (bytes > config_.max_bytes) return MemoryChunkError::CapacityTooSmall;
    try {
        std::lock_guard lock(mutex_);
        if (const auto found = entries_.find(key); found != entries_.end()) {
            bytes_used_ -= found->second.bytes;
            lru_.erase(found->second.lru);
            entries_.erase(found);
        }
        lru_.push_front(key);
        try {
            entries_.emplace(key, Entry{std::move(field), bytes, lru_.begin()});
        } catch (...) {
            lru_.pop_front();
            throw;
        }
        bytes_used_ += bytes;
        trim_locked();
        return MemoryChunkError::None;
    } catch (const std::bad_alloc&) { return MemoryChunkError::AllocationFailed; }
      catch (...) { return MemoryChunkError::AllocationFailed; }
}
std::optional<HeightField> MemoryChunkCache::get(const TerrainTileKey& key) noexcept {
    try {
        std::lock_guard lock(mutex_);
        const auto found = entries_.find(key);
        if (found == entries_.end()) { ++misses_; return std::nullopt; }
        lru_.splice(lru_.begin(), lru_, found->second.lru);
        found->second.lru = lru_.begin();
        ++hits_;
        return found->second.field;
    } catch (...) { return std::nullopt; }
}
bool MemoryChunkCache::erase(const TerrainTileKey& key) noexcept {
    try {
        std::lock_guard lock(mutex_);
        const auto found = entries_.find(key);
        if (found == entries_.end()) return false;
        bytes_used_ -= found->second.bytes;
        lru_.erase(found->second.lru);
        entries_.erase(found);
        return true;
    } catch (...) { return false; }
}
void MemoryChunkCache::trim_locked() noexcept {
    while (bytes_used_ > config_.max_bytes || entries_.size() > config_.max_entries) {
        if (lru_.empty()) break;
        const auto key = lru_.back();
        const auto found = entries_.find(key);
        if (found != entries_.end()) {
            bytes_used_ -= found->second.bytes;
            entries_.erase(found);
            ++evictions_;
        }
        lru_.pop_back();
    }
}
void MemoryChunkCache::clear() noexcept {
    try { std::lock_guard lock(mutex_); entries_.clear(); lru_.clear(); bytes_used_ = 0U; }
    catch (...) {}
}
MemoryChunkCacheStats MemoryChunkCache::stats() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        return {entries_.size(), bytes_used_, config_.max_bytes, hits_, misses_, evictions_};
    } catch (...) { return {}; }
}
} // namespace ldh
