#include "ldh/memory_chunk_cache.hpp"

#include <memory>

#include <pl/Mod.hpp>

namespace ldh::integration {

class LeviDistantHorizonMod final {
public:
    static LeviDistantHorizonMod &instance() {
        static LeviDistantHorizonMod mod;
        return mod;
    }

    LeviDistantHorizonMod() : self_(*ll::mod::NativeMod::current()) {}

    bool load() {
        try {
            ldh::MemoryChunkCacheConfig config{};
            config.max_bytes = 16U * 1024U * 1024U;
            config.max_entry_bytes = 2U * 1024U * 1024U;
            config.max_entries = 256U;
            chunk_cache_ = std::make_unique<ldh::MemoryChunkCache>(config);
            self_.getLogger().info("Levi Distant Horizon {} loaded; native LOD core is ready.", self_.getVersion());
            self_.getLogger().warn("Live Bedrock terrain capture and world rendering are not integrated yet; no game memory hooks are installed.");
            return true;
        } catch (...) {
            self_.getLogger().error("Failed to initialize the Distant Horizon core.");
            chunk_cache_.reset();
            return false;
        }
    }

    bool enable() {
        self_.getLogger().info("Levi Distant Horizon enabled.");
        return chunk_cache_ != nullptr;
    }

    bool disable() {
        if (chunk_cache_) chunk_cache_->clear();
        self_.getLogger().info("Levi Distant Horizon disabled.");
        return true;
    }

    bool unload() {
        if (chunk_cache_) {
            chunk_cache_->clear();
            chunk_cache_.reset();
        }
        self_.getLogger().info("Levi Distant Horizon unloaded.");
        return true;
    }

private:
    ll::mod::NativeMod &self_;
    std::unique_ptr<ldh::MemoryChunkCache> chunk_cache_;
};

} // namespace ldh::integration

PL_REGISTER_MOD(ldh::integration::LeviDistantHorizonMod,
                ldh::integration::LeviDistantHorizonMod::instance())
