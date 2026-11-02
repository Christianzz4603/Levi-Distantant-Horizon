#include "ldh/lod_streamer.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
namespace {
ldh::HeightField flat(std::uint32_t width, std::uint32_t depth) {
    ldh::HeightField field{}; field.width = width; field.depth = depth;
    field.cell_size = 1.0F; field.heights.assign(static_cast<std::size_t>(width) * depth, 5.0F);
    return field;
}
void wait_for_tile(ldh::LodStreamer& streamer, const ldh::TerrainTileKey& key) {
    for (int i = 0; i < 2000; ++i) {
        (void)streamer.poll();
        for (const auto& tile : streamer.resident_tiles())
            if (tile.key == key) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(false && "LOD tile did not finish in time");
}
void streams_and_persists() {
    const auto dir = std::filesystem::temp_directory_path() / "ldh-streamer-tests";
    std::error_code ec; std::filesystem::remove_all(dir, ec);
    ldh::TerrainCache cache({dir, 1024U * 1024U, 512U * 1024U});
    ldh::LodScheduler scheduler({1U, 4U, 4096U});
    ldh::LodStreamer streamer(scheduler, &cache, {4U, 8U, 4096U});
    const ldh::TerrainTileKey key{"world-one", -2, 3, 1U};
    streamer.set_focus("world-one", -2, 3, 2U);
    assert(streamer.submit(key, flat(17U, 17U), 2U, 10, 7U) == ldh::StreamError::None);
    wait_for_tile(streamer, key);
    const auto stats = streamer.stats();
    assert(stats.resident_tiles == 1U && stats.resident_vertices == 81U);
    assert(stats.completed_jobs == 1U);
    assert(streamer.persist(key) == ldh::CacheError::None);
    ldh::LodStreamer restored(scheduler, &cache, {4U, 8U, 4096U});
    restored.set_focus("world-one", -2, 3, 2U);
    assert(restored.restore_cached(key, 7U) == ldh::StreamError::None);
    assert(restored.stats().cache_hits == 1U);
    assert(restored.resident_tiles().front().mesh->indices.size() == 384U);
    streamer.set_focus("another-world", 0, 0, 1U);
    assert(streamer.resident_tiles().empty());
    std::filesystem::remove_all(dir, ec);
}
void rejects_out_of_focus_and_oversized_input() {
    ldh::LodScheduler scheduler({1U, 2U, 1024U});
    ldh::LodStreamer streamer(scheduler, nullptr, {2U, 2U, 256U});
    streamer.set_focus("w", 0, 0, 1U);
    assert(streamer.submit({"w", 3, 0, 0U}, flat(4U, 4U), 1U, 0) == ldh::StreamError::OutOfRange);
    assert(streamer.submit({"w", 0, 0, 0U}, flat(32U, 32U), 1U, 0) == ldh::StreamError::InputTooLarge);
    assert(streamer.submit({"w", 0, 0, 0U}, flat(4U, 4U), 0U, 0) == ldh::StreamError::InvalidSampleStep);
}
void focus_cancels_stale_work() {
    ldh::LodScheduler scheduler({1U, 4U, 65536U});
    ldh::LodStreamer streamer(scheduler, nullptr, {4U, 4U, 65536U});
    streamer.set_focus("w", 0, 0, 2U);
    assert(streamer.submit({"w", 0, 0, 0U}, flat(128U, 128U), 1U, 1) == ldh::StreamError::None);
    streamer.set_focus("w", 100, 100, 1U);
    for (int i = 0; i < 2000 && streamer.stats().pending_tiles != 0U; ++i) {
        (void)streamer.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    (void)streamer.poll();
    assert(streamer.resident_tiles().empty());
}
}
int main() {
    streams_and_persists(); rejects_out_of_focus_and_oversized_input(); focus_cancels_stale_work();
    std::cout << "All LOD streamer tests passed.\n";
}
