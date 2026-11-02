# Levi Distant Horizon

A native Level-of-Detail (LOD) terrain project for LeviLauncher and Minecraft Bedrock on Android.

## Current status

**Milestone 2: bounded asynchronous LOD scheduling.** The portable C++20 core includes heightfield mesh generation, a bounded worker pool, priority ordering, job cancellation, and per-job input budgets. GitHub Actions builds and runs the native tests.

This remains a renderer-independent core. It does not yet hook into Minecraft's world renderer or read live Bedrock chunks.

## Goals

- Render simplified terrain beyond the normal view distance.
- Support configurable LOD coverage, with a long-term target of 1,024 LOD chunks.
- Keep terrain generation off the render thread.
- Cache generated terrain per world.
- Bound memory, queued work, and worker concurrency for Android devices.
- Adapt detail to measured frame time.

A sustained 90 FPS is a performance target, not a universal guarantee. Device GPU, world complexity, thermal throttling, Minecraft version, and renderer integration all affect frame rate.

## Build and test

Requires CMake 3.20+ and a C++20 compiler.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Android build

The core is platform-independent and can be cross-compiled with the Android NDK CMake toolchain. The actual LeviLauncher mod target will be added after the exact Preloader SDK version, headers, sample mod, and supported Minecraft ABI/API are verified.

## Architecture roadmap

1. **LOD core:** heightfield sampling, simplified mesh generation, validation, and tests.
2. **Streaming:** bounded worker pool, prioritized jobs, cancellation, and queue limits.
3. **Cache:** versioned per-world LOD data with size limits and safe invalidation.
4. **Bedrock bridge:** acquire terrain data through supported launcher/game interfaces.
5. **Renderer:** upload and draw LOD meshes with correct GPU resource ownership.
6. **Adaptive quality:** distance bands, frame-time budgeting, and profiling.
7. **Scale testing:** progressively test 64, 128, 256, 512, and 1,024 LOD chunks on real devices.

## Resource-safety notes

- Defaults to two workers and eight queued jobs.
- Worker count is capped at eight.
- Each job has a configurable height-sample budget.
- Queued and running jobs can be cancelled.
- Mesh generation checks cancellation between rows and validates allocation sizes before reserving buffers.

## Compatibility and scope

The LOD core contains no Minecraft code, game assets, or proprietary renderer hooks. Bedrock integration must target documented and verified LeviLauncher Preloader interfaces. Unsupported private offsets or guessed hooks are intentionally avoided.

## License

License and distribution terms will be selected before publishing binary releases.
