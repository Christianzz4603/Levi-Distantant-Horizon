# Levi Distant Horizon

A native Level-of-Detail (LOD) terrain project for LeviLauncher and Minecraft Bedrock on Android.

## Current status

**Milestone 4: asynchronous tile streaming and adaptive quality core.** The C++20 core includes heightfield mesh generation, a bounded priority worker pool, cooperative cancellation, a persistent per-world tile mesh cache, a bounded resident-tile streamer, explicit cache restore/persist operations, and a frame-time quality controller.

The streamer accepts terrain snapshots supplied by a future game/launcher adapter, builds LOD meshes away from the caller thread, drops work outside the current focus region, and keeps a bounded resident set. It does not itself read Minecraft chunks or draw GPU geometry.

## Goals

- Render simplified terrain beyond the normal view distance.
- Support configurable LOD coverage, with a long-term target of 1,024 LOD chunks.
- Keep terrain generation off the render thread.
- Cache generated terrain per world.
- Bound memory, queued work, and disk use for Android devices.
- Adapt detail to measured frame time.

A sustained 90 FPS is a performance target, not a universal guarantee. Device GPU, world complexity, thermal throttling, Minecraft version, and renderer integration all affect frame rate.

## Build and test

Requires CMake 3.20+ and a C++20 compiler.

\`\`\`sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
\`\`\`

## Android build

The platform-independent core is position-independent and can be cross-compiled with the Android NDK CMake toolchain. The official [Preloader Android SDK](https://github.com/LiteLDev/preloader-android) provides native mod lifecycle and mod-menu APIs. The sample verified during this milestone uses SDK tag 0.2.2.

The inspected public Preloader interfaces establish native mod lifecycle and menu/overlay functionality. They do not establish a supported API for acquiring live chunk heightfields or submitting arbitrary 3D terrain meshes to Minecraft's world renderer. Those integrations must target a verified game/renderer interface, not guessed offsets.

## Architecture roadmap

1. **LOD core:** heightfield sampling, simplified mesh generation, validation, and tests.
2. **Streaming:** bounded worker pool, prioritized jobs, cancellation, focus-region eviction, and resident tile limits.
3. **Cache:** versioned per-world LOD meshes with size limits, integrity checks, and explicit background I/O.
4. **Quality control:** frame-time EMA, hysteretic detail reduction/recovery, distance-based sample steps, and work-budget recommendations.
5. **Bedrock bridge:** acquire terrain data through a verified launcher/game interface.
6. **Renderer:** upload and draw LOD meshes with correct GPU resource ownership.
7. **Seams and transitions:** skirts or neighbor-aware stitching, plus stable LOD transitions.
8. **Scale testing:** progressively test 64, 128, 256, 512, and 1,024 LOD chunks on real devices.

## Streaming API notes

- Feed captured tile snapshots to \`LodStreamer::submit\` with a world ID, tile coordinates, LOD level, sample step, priority, and caller-maintained revision.
- Call \`set_focus\` as the player moves. Work and resident tiles outside the square focus radius are cancelled or evicted.
- Call \`poll\` from the update loop. It only consumes completed futures and never waits for worker completion.
- \`restore_cached\` and \`persist\` perform disk I/O. Run them on a background thread, not the render thread.
- Cache restore is explicit because the cache cannot know whether a saved world tile has changed. The game adapter must validate world identity/revision before restoring.
- \`QualityController::observe_frame\` should receive one frame-time sample per frame. Pass its recommended sample step into \`LodStreamer::submit\`.

## Resource-safety notes

- Defaults to two workers and eight queued jobs, with a hard cap of eight workers.
- Each job has a configurable height-sample budget.
- Queued and running jobs can be cancelled between mesh rows.
- Cache files use a versioned binary format and CRC32 integrity checks.
- Cache size and per-entry size are bounded; oldest entries are pruned first.
- Resident meshes and pending tile jobs have independent hard limits.

## Compatibility and scope

The core contains no Minecraft code, game assets, or proprietary renderer hooks. Bedrock integration must target documented and verified interfaces. Existing community mods demonstrate version-specific signature/offset techniques, but those are not treated as a stable public renderer contract by this project.

## License

License and distribution terms will be selected before publishing binary releases.
