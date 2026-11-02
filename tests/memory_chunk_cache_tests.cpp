#include "ldh/memory_chunk_cache.hpp"
#include <cassert>
#include <iostream>
namespace {
ldh::HeightField field(float base = 1.0F) {
    ldh::HeightField value{};
    value.width = 4U; value.depth = 4U; value.cell_size = 1.0F;
    value.heights.assign(16U, base);
    return value;
}
void retains_only_in_memory_and_tracks_lru() {
    ldh::MemoryChunkCache cache({2048U, 1024U, 2U});
    const ldh::TerrainTileKey a{"world", 0, 0, 0U};
    const ldh::TerrainTileKey b{"world", 1, 0, 0U};
    const ldh::TerrainTileKey c{"world", 2, 0, 0U};
    assert(cache.put(a, field(1.0F)) == ldh::MemoryChunkError::None);
    assert(cache.put(b, field(2.0F)) == ldh::MemoryChunkError::None);
    const auto hit = cache.get(a);
    assert(hit && hit->heights.front() == 1.0F);
    assert(cache.put(c, field(3.0F)) == ldh::MemoryChunkError::None);
    assert(!cache.get(b));
    assert(cache.get(a));
    const auto stats = cache.stats();
    assert(stats.entries == 2U && stats.evictions == 1U);
    assert(stats.bytes_used <= stats.capacity_bytes);
    assert(cache.erase(a));
    cache.clear();
    assert(cache.stats().entries == 0U && cache.stats().bytes_used == 0U);
}
void rejects_invalid_and_oversized_snapshots() {
    ldh::MemoryChunkCache cache({1024U, 64U, 8U});
    assert(cache.put({"", 0, 0, 0U}, field()) == ldh::MemoryChunkError::InvalidKey);
    auto bad = field(); bad.heights.pop_back();
    assert(cache.put({"world", 0, 0, 0U}, std::move(bad)) == ldh::MemoryChunkError::InvalidField);
    assert(cache.put({"world", 0, 0, 0U}, field()) == ldh::MemoryChunkError::EntryTooLarge);
}
}
int main() {
    retains_only_in_memory_and_tracks_lru();
    rejects_invalid_and_oversized_snapshots();
    std::cout << "All memory chunk cache tests passed.\\n";
}
