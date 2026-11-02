#include "ldh/terrain_cache.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <limits>
#include <new>
#include <system_error>
#include <utility>
#include <vector>
namespace ldh { namespace {
constexpr std::size_t header_size = 40U;
constexpr std::uint32_t format_version = 1U;
void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    for (unsigned shift=0U; shift<32U; shift+=8U) out.push_back(static_cast<std::uint8_t>((v >> shift) & 0xffU));
}
void patch_u32(std::vector<std::uint8_t>& out, std::size_t at, std::uint32_t v) {
    for (unsigned i=0U; i<4U; ++i) out[at+i]=static_cast<std::uint8_t>((v >> (i*8U)) & 0xffU);
}
bool get_u32(const std::vector<std::uint8_t>& in, std::size_t& at, std::uint32_t& v) noexcept {
    if (at > in.size() || in.size()-at < 4U) return false;
    v=0U; for (unsigned i=0U; i<4U; ++i) v |= static_cast<std::uint32_t>(in[at++]) << (i*8U);
    return true;
}
std::uint32_t crc32(const std::uint8_t* data, std::size_t size) noexcept {
    std::uint32_t crc=0xffffffffU;
    for (std::size_t i=0U; i<size; ++i) {
        crc ^= data[i];
        for (unsigned bit=0U; bit<8U; ++bit) crc=(crc >> 1U) ^ (0xedb88320U & (0U-(crc & 1U)));
    }
    return ~crc;
}
std::uint64_t hash_world(const std::string& value) noexcept {
    std::uint64_t hash=14695981039346656037ULL;
    for (const unsigned char c : value) { hash ^= c; hash *= 1099511628211ULL; }
    return hash;
}
}
TerrainCache::TerrainCache(TerrainCacheConfig config) : config_(std::move(config)) {
    if (config_.max_entry_bytes > config_.max_bytes) config_.max_entry_bytes=config_.max_bytes;
}
bool TerrainCache::valid_key(const TerrainTileKey& key) const noexcept {
    return !key.world_id.empty() && key.world_id.size() <= 128U;
}
std::filesystem::path TerrainCache::path_for(const TerrainTileKey& key) const {
    return config_.directory /
        (std::to_string(hash_world(key.world_id)) + "_" +
         std::to_string(key.x) + "_" + std::to_string(key.z) + "_" +
         std::to_string(key.lod) + ".ldhc");
}
CacheResult TerrainCache::load(const TerrainTileKey& key) const {
    CacheResult out{};
    if (!valid_key(key)) { out.error=CacheError::InvalidKey; return out; }
    std::lock_guard lock(mutex_);
    std::error_code ec;
    const auto path=path_for(key);
    const auto size=std::filesystem::file_size(path,ec);
    if (ec) { out.error=ec==std::errc::no_such_file_or_directory ? CacheError::NotFound : CacheError::IoError; return out; }
    if (size < header_size || size > config_.max_entry_bytes || size > std::numeric_limits<std::size_t>::max()) {
        out.error=CacheError::CorruptData; return out;
    }
    try {
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        std::ifstream file(path,std::ios::binary);
        if (!file || !file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) {
            out.error=CacheError::IoError; return out;
        }
        if (bytes[0]!='L' || bytes[1]!='D' || bytes[2]!='H' || bytes[3]!='C') { out.error=CacheError::CorruptData; return out; }
        std::size_t at=4U; std::uint32_t version=0U, world_len=0U, x=0U, z=0U, lod=0U, vc=0U, ic=0U, checksum=0U;
        if (!get_u32(bytes,at,version)||!get_u32(bytes,at,world_len)||!get_u32(bytes,at,x)||!get_u32(bytes,at,z)||
            !get_u32(bytes,at,lod)||!get_u32(bytes,at,vc)||!get_u32(bytes,at,ic)||!get_u32(bytes,at,checksum) ||
            version!=format_version || world_len==0U || world_len>128U) { out.error=CacheError::CorruptData; return out; }
        const std::size_t payload=header_size;
        if (world_len > bytes.size()-payload) { out.error=CacheError::CorruptData; return out; }
        const std::size_t world_end=payload+world_len;
        const std::size_t vertex_bytes=static_cast<std::size_t>(vc)*12U;
        const std::size_t index_bytes=static_cast<std::size_t>(ic)*4U;
        if (vc > std::numeric_limits<std::size_t>::max()/12U || ic > std::numeric_limits<std::size_t>::max()/4U ||
            vertex_bytes > bytes.size()-world_end || index_bytes != bytes.size()-world_end-vertex_bytes ||
            crc32(bytes.data()+payload,bytes.size()-payload)!=checksum) { out.error=CacheError::CorruptData; return out; }
        const std::string world(reinterpret_cast<const char*>(bytes.data()+payload),world_len);
        if (world!=key.world_id || std::bit_cast<std::int32_t>(x)!=key.x ||
            std::bit_cast<std::int32_t>(z)!=key.z || lod!=key.lod) { out.error=CacheError::NotFound; return out; }
        out.mesh.vertices.reserve(vc); out.mesh.indices.reserve(ic);
        at=world_end;
        for (std::uint32_t i=0U;i<vc;++i) {
            std::uint32_t bx=0U,by=0U,bz=0U;
            if (!get_u32(bytes,at,bx)||!get_u32(bytes,at,by)||!get_u32(bytes,at,bz)) { out.mesh={}; out.error=CacheError::CorruptData; return out; }
            Vertex v{std::bit_cast<float>(bx),std::bit_cast<float>(by),std::bit_cast<float>(bz)};
            if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) { out.mesh={}; out.error=CacheError::CorruptData; return out; }
            out.mesh.vertices.push_back(v);
        }
        for (std::uint32_t i=0U;i<ic;++i) {
            std::uint32_t index=0U;
            if (!get_u32(bytes,at,index)||index>=vc) { out.mesh={}; out.error=CacheError::CorruptData; return out; }
            out.mesh.indices.push_back(index);
        }
    } catch (const std::bad_alloc&) { out.mesh={}; out.error=CacheError::AllocationFailed; }
    return out;
}
CacheError TerrainCache::store(const TerrainTileKey& key,const Mesh& mesh) {
    if (!valid_key(key) || config_.directory.empty() ||
        mesh.vertices.size()>std::numeric_limits<std::uint32_t>::max() ||
        mesh.indices.size()>std::numeric_limits<std::uint32_t>::max()) return CacheError::InvalidKey;
    std::lock_guard lock(mutex_);
    try {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(header_size+key.world_id.size()+mesh.vertices.size()*12U+mesh.indices.size()*4U);
        bytes.insert(bytes.end(),{'L','D','H','C'});
        put_u32(bytes,format_version); put_u32(bytes,static_cast<std::uint32_t>(key.world_id.size()));
        put_u32(bytes,std::bit_cast<std::uint32_t>(key.x)); put_u32(bytes,std::bit_cast<std::uint32_t>(key.z));
        put_u32(bytes,key.lod); put_u32(bytes,static_cast<std::uint32_t>(mesh.vertices.size()));
        put_u32(bytes,static_cast<std::uint32_t>(mesh.indices.size())); put_u32(bytes,0U);
        bytes.insert(bytes.end(),key.world_id.begin(),key.world_id.end());
        for (const auto& v:mesh.vertices) {
            if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) return CacheError::CorruptData;
            put_u32(bytes,std::bit_cast<std::uint32_t>(v.x)); put_u32(bytes,std::bit_cast<std::uint32_t>(v.y)); put_u32(bytes,std::bit_cast<std::uint32_t>(v.z));
        }
        for (const auto index:mesh.indices) {
            if (index>=mesh.vertices.size()) return CacheError::CorruptData;
            put_u32(bytes,index);
        }
        patch_u32(bytes,36U,crc32(bytes.data()+header_size,bytes.size()-header_size));
        if (bytes.size()>config_.max_entry_bytes || bytes.size()>config_.max_bytes) return CacheError::EntryTooLarge;
        std::error_code ec;
        std::filesystem::create_directories(config_.directory,ec);
        if (ec) return CacheError::IoError;
        const auto target=path_for(key);
        auto temp=target; temp += ".tmp";
        { std::ofstream file(temp,std::ios::binary|std::ios::trunc);
          if (!file || !file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))) {
              std::filesystem::remove(temp,ec); return CacheError::IoError;
          }
          file.flush(); if (!file) { std::filesystem::remove(temp,ec); return CacheError::IoError; }
        }
        std::filesystem::rename(temp,target,ec);
        if (ec) { ec.clear(); std::filesystem::remove(target,ec); ec.clear(); std::filesystem::rename(temp,target,ec); }
        if (ec) { std::filesystem::remove(temp,ec); return CacheError::IoError; }
        (void)prune();
    } catch (const std::bad_alloc&) { return CacheError::AllocationFailed; }
    catch (const std::filesystem::filesystem_error&) { return CacheError::IoError; }
    return CacheError::None;
}
std::uint64_t TerrainCache::bytes_used() const noexcept {
    std::lock_guard lock(mutex_); std::uint64_t total=0U; std::error_code ec;
    if (!std::filesystem::exists(config_.directory,ec)) return 0U;
    for (const auto& entry:std::filesystem::directory_iterator(config_.directory,ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && entry.path().extension()==".ldhc") {
            const auto size=entry.file_size(ec); if (!ec && size<=std::numeric_limits<std::uint64_t>::max()-total) total+=size;
        }
        ec.clear();
    }
    return total;
}
bool TerrainCache::prune() noexcept {
    std::lock_guard lock(mutex_);
    struct Item { std::filesystem::path path; std::uint64_t size; std::filesystem::file_time_type time; };
    std::vector<Item> items; std::uint64_t total=0U; std::error_code ec;
    try {
        if (!std::filesystem::exists(config_.directory,ec)) return true;
        for (const auto& entry:std::filesystem::directory_iterator(config_.directory,ec)) {
            if (ec) return false;
            if (!entry.is_regular_file(ec)||entry.path().extension()!=".ldhc") { ec.clear(); continue; }
            const auto size=entry.file_size(ec); if (ec) return false;
            const auto time=entry.last_write_time(ec); if (ec) return false;
            items.push_back({entry.path(),size,time});
            if (size>std::numeric_limits<std::uint64_t>::max()-total) return false;
            total+=size;
        }
        std::sort(items.begin(),items.end(),[](const Item& a,const Item& b){return a.time<b.time;});
        for (const auto& item:items) {
            if (total<=config_.max_bytes) break;
            std::filesystem::remove(item.path,ec); if (ec) return false;
            total-=item.size;
        }
    } catch (...) { return false; }
    return total<=config_.max_bytes;
}
} // namespace ldh
