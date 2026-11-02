#include "ldh/terrain_cache.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
namespace {
void round_trip() {
    const auto dir=std::filesystem::temp_directory_path()/"ldh-cache-roundtrip";
    std::error_code ec; std::filesystem::remove_all(dir,ec);
    ldh::TerrainCache cache({dir,1024U*1024U,512U*1024U});
    ldh::TerrainTileKey key{"world-alpha",-12,31,2U};
    ldh::Mesh mesh{}; mesh.vertices={{0,1,0},{1,2,0},{0,3,1}}; mesh.indices={0,1,2};
    assert(cache.store(key,mesh)==ldh::CacheError::None);
    const auto loaded=cache.load(key); assert(loaded); assert(loaded.mesh.vertices.size()==3U);
    assert(loaded.mesh.indices.size()==3U); assert(loaded.mesh.vertices[1].y==2.0F);
    auto other=key; other.world_id="another-world"; assert(cache.load(other).error==ldh::CacheError::NotFound);
    assert(cache.bytes_used()>0U);
    std::filesystem::remove_all(dir,ec);
}
void rejects_bad_mesh() {
    const auto dir=std::filesystem::temp_directory_path()/"ldh-cache-invalid";
    std::error_code ec; std::filesystem::remove_all(dir,ec);
    ldh::TerrainCache cache({dir,1024U,512U});
    ldh::TerrainTileKey key{"world",0,0,0U};
    ldh::Mesh mesh{}; mesh.vertices={{0,0,0}}; mesh.indices={4U};
    assert(cache.store(key,mesh)==ldh::CacheError::CorruptData);
    std::filesystem::remove_all(dir,ec);
}
}
int main() { round_trip(); rejects_bad_mesh(); std::cout<<"All terrain cache tests passed.\n"; }
