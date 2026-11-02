# Levi Distant Horizon

A native LeviLauncher mod project for Minecraft Bedrock on Android, targeting the **26.60+** development line.

## LeviLauncher package

The repository builds a native mod package using the public Preloader Android SDK (0.2.2).

- Package format: `.levipack`
- Runtime mod ID: `levi_distant_horizon`
- ABI: `arm64-v8a`
- Native entry: `liblevi_distant_horizon.so`
- Loader manifest type: `preload-native`

GitHub Actions builds and validates the package. Import the generated `.levipack` into LeviLauncher.

## New settings

The mod now registers four editable Mod Menu modules and persists their values in `config/config.json`:

### LOD Chunks
- Enable/disable the LOD module.
- Set LOD radius from 8 to 256 chunks.
- Adjust mesh detail step from 1 to 16.
- Toggle adaptive quality.

### Fake Distant Chunks
- Enable/disable the distant fake-chunk profile.
- Set the configured distance from 16 to 512 chunks.
- Adjust fake-chunk detail step from 1 to 16.

### Bobby Chunk Cache
- Enable/disable the Bobby-inspired RAM snapshot cache.
- Set its memory budget from 4 to 256 MiB.
- Set its maximum entry count from 32 to 4096.

### Input Diagnostics
- Passively observes Android touch down, up, move, pointer, and cancel events through the public Preloader Input API.
- Tracks unmatched releases, duplicate downs, active pointers, and long-held pointers.
- Prints a compact summary to the mod log when the module is disabled or the mod unloads.
- Never consumes or synthesizes touch events, changes attack timing, alters block placement cadence, or modifies server-authoritative combat.

Settings are available in LeviLauncher’s native Mod Menu and are saved automatically. The Bobby RAM cache and passive input diagnostics are connected to live components. Input diagnostics are measurement-only: they can reveal missing or unusually long touch sequences, but cannot repair Minecraft’s internal button state through the current public callback API.

## Important implementation status

This is a loadable Preloader-native module with a renderer-independent C++20 LOD core. **It is not yet a functioning Distant Horizons renderer.** The public lifecycle and Mod Menu APIs provide native mod loading and configuration, but the project still needs a verified Minecraft adapter to capture loaded Bedrock chunks and submit generated meshes to the world renderer.

Consequently:
- LOD radius, LOD detail, adaptive quality, and fake-chunk distance/detail are persisted settings for the future terrain adapter. They do not currently change what appears in the world.
- The Bobby-inspired RAM cache is configurable and initialized, but it receives no terrain snapshots until a verified chunk-capture adapter exists.
- No private game offsets or unverified hooks are installed. The current Preloader 0.2.2 Input API exposes observation/consumption callbacks but no supported API for synthesizing a missing touch-up event or resetting Minecraft’s internal attack-button state.
- An empty `minecraft_versions` manifest allowlist is not a claim of compatibility with every game build. Runtime compatibility must be verified against the matching LeviLauncher/Preloader and Minecraft build.

## Native core

The renderer-independent C++20 core currently provides heightfield-to-LOD mesh generation, bounded priority scheduling and cooperative cancellation, a persistent per-world terrain mesh cache, bounded LOD tile streaming, adaptive quality control, optional boundary skirts, and a bounded RAM-only LRU terrain snapshot cache.

## Build and test the core on Linux

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Build the Android LeviPack locally

Requirements: Android NDK 28.2.13676358, CMake 3.22+, and Ninja.

```sh
cmake -S . -B build-android -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-28 \
  -DLDH_BUILD_TESTS=OFF \
  -DLDH_BUILD_LEVI_MOD=ON
cmake --build build-android --target levi_package --parallel 2
```

The package is written to `build-android/Levi-Distant-Horizon-0.8.0-arm64-v8a.levipack`.

## Roadmap

1. Verify the native package loads in LeviLauncher.
2. Identify a supported, version-verified way to capture loaded terrain.
3. Feed captured heightfields into the RAM cache and LOD streamer.
4. Verify 26.60 game-thread, block-update, and renderer hook points before attempting RenderSync.
5. Investigate safe touch-state recovery and frame-pacing integration; do not bypass server-authoritative combat timing.
6. Integrate mesh upload and rendering through a verified game-renderer interface.
7. Test input, rendering, transitions, memory limits, and frame-time behavior on low-end Android devices.
