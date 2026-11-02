# Levi Distant Horizon

A native Level-of-Detail (LOD) terrain project for LeviLauncher and Minecraft Bedrock on Android.

## Current status

**Milestone 1: portable LOD mesh core.** This repository currently contains a renderer-independent C++20 heightfield mesh simplifier and automated tests. It does not yet hook into Minecraft's world renderer or read live Bedrock chunks.

## Goals

- Render simplified terrain beyond the normal view distance.
- Support configurable LOD coverage with a long-term target of 1,024 LOD chunks.
- Keep terrain generation off the render thread.
- Cache generated terrain per world.
- Bound memory use, work queues, and GPU uploads for Android devices.
- Adapt detail to measured frame time.

A sustained 90 FPS is a performance target, not a universal guarantee. Device GPU, world complexity, thermal throttling, Minecraft version, and renderer integration all affect frame rate.

## Build and test

Requires CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Android build

The core is platform-independent and can be cross-compiled with the Android NDK CMake toolchain. The actual LeviLauncher mod target will be added after the exact Preloader SDK version, headers, sample mod, and supported Minecraft ABI/API are verified.

## Architecture roadmap

1. **LOD core:** heightfield sampling, simplified mesh generation, validation and tests.
2. **Streaming:** bounded worker pool, prioritized tile queue and cancellation.
3. **Cache:** versioned per-world LOD data with size limits and safe invalidation.
4. **Bedrock bridge:** acquire terrain data through supported launcher/game interfaces.
5. **Renderer:** upload and draw LOD meshes with correct GPU resource ownership.
6. **Adaptive quality:** distance bands, frame-time budgeting and profiling.
7. **Scale testing:** progressively test 64, 128, 256, 512 and 1,024 LOD chunks on real devices.

## Scope and compatibility

The LOD mesh core does not contain Minecraft code, game assets, or proprietary renderer hooks. Bedrock integration must be implemented against documented and verified LeviLauncher Preloader interfaces. Unsupported private offsets or guessed hooks are intentionally avoided.

## License

License and distribution terms will be selected before publishing binary releases.
