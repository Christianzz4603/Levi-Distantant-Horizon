# Levi Distant Horizon

A native LeviLauncher mod project for Minecraft Bedrock on Android, targeting the 26.60+ development line.

## LeviLauncher package

The repository builds a real Android native mod package using the public Preloader Android SDK (0.2.2).

- Package format: `.levipack`
- Runtime mod ID: `levi_distant_horizon`
- Display name: Levi Distant Horizon
- ABI: `arm64-v8a`
- Native entry: `liblevi_distant_horizon.so`
- Loader manifest type: `preload-native`

GitHub Actions builds and validates the package on pushes to `main`, pull requests, and manual dispatches. Download the `Levi-Distant-Horizon-levipack-arm64` artifact from the workflow run, then import the `.levipack` into LeviLauncher.

## Current implementation status

This is a genuine Preloader-native module, not an APK or a resource/behavior pack. Its lifecycle loads the C++20 LOD core and creates the bounded 16 MiB RAM-only chunk snapshot cache. The cache is cleared when the mod is disabled or unloaded.

**Important:** this is the first loadable integration milestone, not a finished Distant Horizons renderer. The public Preloader SDK lifecycle does not itself provide a supported API for reading live Bedrock chunk heightfields or submitting arbitrary terrain meshes to the game's world renderer. This mod therefore does not claim to display distant terrain yet. It logs this limitation rather than guessing private game offsets or installing unsafe hooks.

The Bobby-inspired cache only retains snapshots supplied to it by a future verified game adapter. It does not discover terrain that Minecraft has never loaded, and it writes no snapshots to disk.

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

The package is written to `build-android/Levi-Distant-Horizon-0.7.0-arm64-v8a.levipack`.

## Minecraft compatibility

The native module currently declares an empty `minecraft_versions` allowlist, which the Preloader manifest defines as all versions. That is not a claim that every game build is compatible. Runtime compatibility must be verified against the matching LeviLauncher/Preloader and Minecraft build. The project target remains Bedrock 26.60+.

## Roadmap

1. Verify the native package loads in LeviLauncher.
2. Identify a supported, version-verified way to capture loaded terrain.
3. Feed captured heightfields into the RAM cache and LOD streamer.
4. Integrate mesh upload and rendering through a verified game-renderer interface.
5. Test transitions, memory limits, and frame-time behavior on low-end Android devices.
