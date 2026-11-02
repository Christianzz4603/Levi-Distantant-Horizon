#include "ldh/memory_chunk_cache.hpp"
#include "ldh/input_diagnostics.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include <pl/Config.hpp>
#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>
#include <pl/Input.hpp>

namespace ldh::integration {
namespace {

constexpr std::string_view kLodModuleId = "levi_distant_horizon.lod";
constexpr std::string_view kFakeModuleId = "levi_distant_horizon.fake_chunks";
constexpr std::string_view kBobbyModuleId = "levi_distant_horizon.bobby_cache";
constexpr std::string_view kInputModuleId = "levi_distant_horizon.input_diagnostics";

struct LeviSettings {
    int version = 1;

    bool lodEnabled = true;
    int lodChunkRadius = 48;
    int lodDetailStep = 2;
    bool adaptiveQuality = true;

    bool fakeChunksEnabled = true;
    int fakeChunkDistance = 96;
    int fakeChunkDetail = 4;

    bool bobbyCacheEnabled = true;
    int bobbyCacheMemoryMb = 16;
    int bobbyCacheMaxEntries = 256;

    bool inputDiagnosticsEnabled = true;
    int stalePointerThresholdMs = 2500;
};

int parseInt(std::string_view value, int fallback) {
    const std::string text(value);
    char *end = nullptr;
    errno = 0;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || errno == ERANGE ||
        parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool parseBool(std::string_view value, bool fallback) {
    if (value == "true" || value == "1" || value == "on") return true;
    if (value == "false" || value == "0" || value == "off") return false;
    return fallback;
}

void normalize(LeviSettings &settings) {
    settings.version = 1;
    settings.lodChunkRadius = std::clamp(settings.lodChunkRadius, 8, 256);
    settings.lodDetailStep = std::clamp(settings.lodDetailStep, 1, 16);
    settings.fakeChunkDistance = std::clamp(settings.fakeChunkDistance, 16, 512);
    settings.fakeChunkDetail = std::clamp(settings.fakeChunkDetail, 1, 16);
    settings.bobbyCacheMemoryMb = std::clamp(settings.bobbyCacheMemoryMb, 4, 256);
    settings.bobbyCacheMaxEntries = std::clamp(settings.bobbyCacheMaxEntries, 32, 4096);
    settings.stalePointerThresholdMs = std::clamp(settings.stalePointerThresholdMs, 250, 10000);
}

} // namespace

class LeviDistantHorizonMod final {
public:
    static LeviDistantHorizonMod &instance() {
        static LeviDistantHorizonMod mod;
        return mod;
    }

    LeviDistantHorizonMod() = default;

    bool load() {
        self_ = ll::mod::NativeMod::current();
        if (!self_) return false;
        try {
            std::lock_guard lock(config_mutex_);
            config_file_.emplace();
            if (!config_file_->load()) {
                self_->getLogger().error("Could not load Levi Distant Horizon settings.");
                config_file_.reset();
                return false;
            }

            normalize(config_file_->value());
            if (!config_file_->save()) {
                self_->getLogger().warn("Could not normalize and save settings.");
            }

            rebuildCacheLocked();
            input_diagnostics_.reset();
            input_diagnostics_.setStaleThreshold(
                std::chrono::milliseconds(config_file_->value().stalePointerThresholdMs));
            input_diagnostics_.setEnabled(config_file_->value().inputDiagnosticsEnabled);
            registerInputCallbackOnce();
            self_->getLogger().info("Levi Distant Horizon {} loaded.", self_->getVersion());
            self_->getLogger().warn(
                "Settings are saved and editable, but live chunk capture and terrain rendering "
                "still require a verified Minecraft adapter.");
            return true;
        } catch (...) {
            self_->getLogger().error("Failed to initialize Levi Distant Horizon.");
            chunk_cache_.reset();
            config_file_.reset();
            return false;
        }
    }

    bool enable() {
        self_ = ll::mod::NativeMod::current();
        if (!self_) return false;
        const LeviSettings settings = snapshotSettings();

        const bool lodRegistered =
            pl::modmenu::ModuleBuilder(std::string(kLodModuleId), "LOD Chunks")
                .modId(self_->getId())
                .description("Configure the distant LOD chunk radius and mesh detail.")
                .defaultEnabled(settings.lodEnabled)
                .onToggle(onModuleToggle)
                .config("lodChunkRadius", "LOD Radius (chunks)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.lodChunkRadius), "8", "256")
                .config("lodDetailStep", "LOD Detail Step",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.lodDetailStep), "1", "16")
                .config("adaptiveQuality", "Adaptive Quality",
                        pl::modmenu::ConfigType::Toggle,
                        settings.adaptiveQuality ? "true" : "false")
                .onConfigChanged(onConfigChanged)
                .registerModule();

        const bool fakeRegistered =
            pl::modmenu::ModuleBuilder(std::string(kFakeModuleId), "Fake Distant Chunks")
                .modId(self_->getId())
                .description("Distant-Horizons-style simplified chunk distance and detail settings.")
                .defaultEnabled(settings.fakeChunksEnabled)
                .onToggle(onModuleToggle)
                .config("fakeChunkDistance", "Fake Chunk Distance (chunks)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.fakeChunkDistance), "16", "512")
                .config("fakeChunkDetail", "Fake Chunk Detail Step",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.fakeChunkDetail), "1", "16")
                .onConfigChanged(onConfigChanged)
                .registerModule();

        const bool bobbyRegistered =
            pl::modmenu::ModuleBuilder(std::string(kBobbyModuleId), "Bobby Chunk Cache")
                .modId(self_->getId())
                .description("Configure the Bobby-inspired in-memory chunk snapshot cache.")
                .defaultEnabled(settings.bobbyCacheEnabled)
                .onToggle(onModuleToggle)
                .config("bobbyCacheMemoryMb", "RAM Cache Budget (MiB)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.bobbyCacheMemoryMb), "4", "256")
                .config("bobbyCacheMaxEntries", "Maximum Cached Chunks",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.bobbyCacheMaxEntries), "32", "4096")
                .onConfigChanged(onConfigChanged)
                .registerModule();

        const bool inputRegistered =
            pl::modmenu::ModuleBuilder(std::string(kInputModuleId), "Input Diagnostics")
                .modId(self_->getId())
                .description("Passively tracks touch down/up/cancel events to diagnose stuck controls. Does not alter input or combat timing.")
                .defaultEnabled(settings.inputDiagnosticsEnabled)
                .onToggle(onModuleToggle)
                .config("stalePointerThresholdMs", "Long-held Touch Warning (ms)",
                        pl::modmenu::ConfigType::SliderInt,
                        std::to_string(settings.stalePointerThresholdMs), "250", "10000")
                .onConfigChanged(onConfigChanged)
                .registerModule();

        const bool registered = lodRegistered && fakeRegistered && bobbyRegistered && inputRegistered;
        if (!registered) {
            unregisterSettings();
            self_->getLogger().error(
                "Settings registration failed (LOD={}, fake chunks={}, Bobby={}, input diagnostics={}).",
                lodRegistered, fakeRegistered, bobbyRegistered, inputRegistered);
            return false;
        }

        input_diagnostics_.setStaleThreshold(std::chrono::milliseconds(settings.stalePointerThresholdMs));
        input_diagnostics_.setEnabled(settings.inputDiagnosticsEnabled);
        self_->getLogger().info("Registered LOD, fake chunk, Bobby cache, and input diagnostics settings.");
        self_->getLogger().warn(
            "The settings currently configure saved mod state and the RAM cache; "
            "they do not activate distant terrain rendering without the game adapter.");
        return true;
    }

    bool disable() {
        self_ = ll::mod::NativeMod::current();
        if (!self_) return false;
        unregisterSettings();
        input_diagnostics_.setEnabled(false);
        logInputSummary();
        input_diagnostics_.reset();
        std::lock_guard lock(config_mutex_);
        if (chunk_cache_) chunk_cache_->clear();
        self_->getLogger().info("Levi Distant Horizon disabled.");
        return true;
    }

    bool unload() {
        self_ = ll::mod::NativeMod::current();
        if (!self_) return false;
        unregisterSettings();
        input_diagnostics_.setEnabled(false);
        logInputSummary();
        input_diagnostics_.reset();
        std::lock_guard lock(config_mutex_);
        if (chunk_cache_) {
            chunk_cache_->clear();
            chunk_cache_.reset();
        }
        config_file_.reset();
        self_->getLogger().info("Levi Distant Horizon unloaded.");
        return true;
    }

private:
    ll::mod::NativeMod *self_{};
    std::mutex config_mutex_;
    std::optional<pl::config::ConfigFile<LeviSettings>> config_file_;
    std::unique_ptr<ldh::MemoryChunkCache> chunk_cache_;
    ldh::InputDiagnostics input_diagnostics_;
    std::once_flag input_callback_once_;

    LeviSettings snapshotSettings() {
        std::lock_guard lock(config_mutex_);
        if (!config_file_) return {};
        LeviSettings snapshot = config_file_->value();
        normalize(snapshot);
        return snapshot;
    }

    void registerInputCallbackOnce() {
        std::call_once(input_callback_once_, [] {
            pl::input::registerTouchCallback([](const pl::input::TouchEvent &event) {
                auto &mod = instance();
                mod.input_diagnostics_.observe(event.action, event.pointerId);
                return false; // Observe only. Never consume gameplay touch events.
            });
        });
    }

    void logInputSummary() {
        if (!self_) return;
        const auto stats = input_diagnostics_.snapshot();
        self_->getLogger().info(
            "Touch diagnostics: events={}, down={}, up={}, cancel={}, unmatched-up={}, duplicate-down={}, active={}, possibly-stuck={}, avg-press={}ms, max-press={}ms",
            stats.events, stats.downs, stats.ups, stats.cancels, stats.unmatchedUps,
            stats.duplicateDowns, stats.activePointers, stats.possiblyStuckPointers,
            stats.averagePressDurationMs, stats.maximumPressDurationMs);
    }

    void rebuildCacheLocked() {
        if (!config_file_ || !config_file_->value().bobbyCacheEnabled) {
            chunk_cache_.reset();
            return;
        }

        const auto &settings = config_file_->value();
        ldh::MemoryChunkCacheConfig cache_config{};
        cache_config.max_bytes =
            static_cast<std::size_t>(settings.bobbyCacheMemoryMb) * 1024U * 1024U;
        cache_config.max_entry_bytes =
            std::min<std::size_t>(cache_config.max_bytes, 2U * 1024U * 1024U);
        cache_config.max_entries =
            static_cast<std::size_t>(settings.bobbyCacheMaxEntries);
        chunk_cache_ = std::make_unique<ldh::MemoryChunkCache>(cache_config);
    }

    bool saveLocked(std::string_view reason) {
        if (!config_file_) return false;
        normalize(config_file_->value());
        const bool saved = config_file_->save();
        if (saved) {
            self_->getLogger().info("Saved settings after {}.", reason);
        } else {
            self_->getLogger().warn("Could not save settings after {}.", reason);
        }
        return saved;
    }

    static void onModuleToggle(std::string_view module_id, bool enabled) {
        instance().handleModuleToggle(module_id, enabled);
    }

    static void onConfigChanged(std::string_view module_id,
                                std::string_view key,
                                std::string_view value) {
        instance().handleConfigChanged(module_id, key, value);
    }

    void handleModuleToggle(std::string_view module_id, bool enabled) {
        std::lock_guard lock(config_mutex_);
        if (!config_file_) return;
        auto &settings = config_file_->value();

        if (module_id == kLodModuleId) {
            settings.lodEnabled = enabled;
        } else if (module_id == kFakeModuleId) {
            settings.fakeChunksEnabled = enabled;
        } else if (module_id == kBobbyModuleId) {
            settings.bobbyCacheEnabled = enabled;
            try {
                rebuildCacheLocked();
            } catch (...) {
                settings.bobbyCacheEnabled = false;
                chunk_cache_.reset();
                self_->getLogger().error("Could not resize the Bobby cache.");
            }
        } else if (module_id == kInputModuleId) {
            settings.inputDiagnosticsEnabled = enabled;
            input_diagnostics_.setEnabled(enabled);
            if (!enabled) {
                logInputSummary();
                input_diagnostics_.reset();
            }
        } else {
            return;
        }
        saveLocked("module toggle");
    }

    void handleConfigChanged(std::string_view module_id,
                             std::string_view key,
                             std::string_view value) {
        std::lock_guard lock(config_mutex_);
        if (!config_file_) return;
        auto &settings = config_file_->value();
        bool cache_changed = false;

        if (module_id == kLodModuleId) {
            if (key == "lodChunkRadius") {
                settings.lodChunkRadius = parseInt(value, settings.lodChunkRadius);
            } else if (key == "lodDetailStep") {
                settings.lodDetailStep = parseInt(value, settings.lodDetailStep);
            } else if (key == "adaptiveQuality") {
                settings.adaptiveQuality = parseBool(value, settings.adaptiveQuality);
            } else {
                return;
            }
        } else if (module_id == kFakeModuleId) {
            if (key == "fakeChunkDistance") {
                settings.fakeChunkDistance = parseInt(value, settings.fakeChunkDistance);
            } else if (key == "fakeChunkDetail") {
                settings.fakeChunkDetail = parseInt(value, settings.fakeChunkDetail);
            } else {
                return;
            }
        } else if (module_id == kBobbyModuleId) {
            if (key == "bobbyCacheMemoryMb") {
                settings.bobbyCacheMemoryMb = parseInt(value, settings.bobbyCacheMemoryMb);
                cache_changed = true;
            } else if (key == "bobbyCacheMaxEntries") {
                settings.bobbyCacheMaxEntries = parseInt(value, settings.bobbyCacheMaxEntries);
                cache_changed = true;
            } else {
                return;
            }
        } else if (module_id == kInputModuleId) {
            if (key == "stalePointerThresholdMs") {
                settings.stalePointerThresholdMs = parseInt(value, settings.stalePointerThresholdMs);
            } else {
                return;
            }
        } else {
            return;
        }

        normalize(settings);
        if (module_id == kInputModuleId) {
            input_diagnostics_.setStaleThreshold(
                std::chrono::milliseconds(settings.stalePointerThresholdMs));
        }
        if (cache_changed) {
            try {
                rebuildCacheLocked();
            } catch (...) {
                settings.bobbyCacheEnabled = false;
                chunk_cache_.reset();
                self_->getLogger().error("Could not apply the new Bobby cache limits.");
            }
        }
        saveLocked(key);
    }

    void unregisterSettings() {
        pl::modmenu::unregisterModule(kLodModuleId);
        pl::modmenu::unregisterModule(kFakeModuleId);
        pl::modmenu::unregisterModule(kBobbyModuleId);
        pl::modmenu::unregisterModule(kInputModuleId);
    }
};

} // namespace ldh::integration

PL_REGISTER_MOD(ldh::integration::LeviDistantHorizonMod,
                ldh::integration::LeviDistantHorizonMod::instance())

