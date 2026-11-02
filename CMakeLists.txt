cmake_minimum_required(VERSION 3.22)
project(LeviDistantHorizon VERSION 0.7.0 LANGUAGES CXX)

option(LDH_BUILD_TESTS "Build core tests" ON)
option(LDH_BUILD_LEVI_MOD "Build the Android LeviLauncher native mod and .levipack" OFF)

add_library(ldh_core
  src/lod_mesh.cpp
  src/lod_scheduler.cpp
  src/terrain_cache.cpp
  src/lod_streamer.cpp
  src/quality_controller.cpp
  src/mesh_seams.cpp
  src/memory_chunk_cache.cpp
)
target_include_directories(ldh_core PUBLIC include)
target_compile_features(ldh_core PUBLIC cxx_std_20)
set_target_properties(ldh_core PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(ldh_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(ldh_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
find_package(Threads REQUIRED)
target_link_libraries(ldh_core PUBLIC Threads::Threads)

if(LDH_BUILD_TESTS AND NOT ANDROID)
  enable_testing()
  add_executable(ldh_core_tests tests/lod_mesh_tests.cpp)
  target_link_libraries(ldh_core_tests PRIVATE ldh_core)
  add_test(NAME ldh_core_tests COMMAND ldh_core_tests)
  add_executable(ldh_scheduler_tests tests/lod_scheduler_tests.cpp)
  target_link_libraries(ldh_scheduler_tests PRIVATE ldh_core)
  add_test(NAME ldh_scheduler_tests COMMAND ldh_scheduler_tests)
  add_executable(ldh_cache_tests tests/terrain_cache_tests.cpp)
  target_link_libraries(ldh_cache_tests PRIVATE ldh_core)
  add_test(NAME ldh_cache_tests COMMAND ldh_cache_tests)
  add_executable(ldh_streamer_tests tests/lod_streamer_tests.cpp)
  target_link_libraries(ldh_streamer_tests PRIVATE ldh_core)
  add_test(NAME ldh_streamer_tests COMMAND ldh_streamer_tests)
  add_executable(ldh_quality_tests tests/quality_controller_tests.cpp)
  target_link_libraries(ldh_quality_tests PRIVATE ldh_core)
  add_test(NAME ldh_quality_tests COMMAND ldh_quality_tests)
  add_executable(ldh_seam_tests tests/mesh_seams_tests.cpp)
  target_link_libraries(ldh_seam_tests PRIVATE ldh_core)
  add_test(NAME ldh_seam_tests COMMAND ldh_seam_tests)
  add_executable(ldh_memory_chunk_cache_tests tests/memory_chunk_cache_tests.cpp)
  target_link_libraries(ldh_memory_chunk_cache_tests PRIVATE ldh_core)
  add_test(NAME ldh_memory_chunk_cache_tests COMMAND ldh_memory_chunk_cache_tests)
endif()

if(LDH_BUILD_LEVI_MOD)
  if(NOT ANDROID)
    message(FATAL_ERROR "LDH_BUILD_LEVI_MOD requires the Android NDK toolchain.")
  endif()
  if(NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Levi Distant Horizon currently packages arm64-v8a only.")
  endif()

  include(FetchContent)
  FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    GIT_TAG 0.2.2
  )
  FetchContent_MakeAvailable(preloader_android)

  add_library(levi_distant_horizon SHARED src/levi_mod.cpp)
  target_include_directories(levi_distant_horizon PRIVATE include)
  target_compile_features(levi_distant_horizon PRIVATE cxx_std_20)
  target_compile_options(levi_distant_horizon PRIVATE
    -Wall -Wextra -Wpedantic
    -fvisibility=hidden
    -fvisibility-inlines-hidden
  )
  target_link_options(levi_distant_horizon PRIVATE "-Wl,-z,max-page-size=16384")
  target_link_libraries(levi_distant_horizon PRIVATE ldh_core preloader)

  set(LEVI_MOD_ID "levi_distant_horizon")
  set(LEVI_MOD_NAME "Levi Distant Horizon")
  set(LEVI_MOD_AUTHOR "Christianzz4603")
  set(LEVI_MOD_VERSION "${PROJECT_VERSION}")
  set(LEVI_PACKAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/package")
  set(LEVI_MOD_DIR "${LEVI_PACKAGE_DIR}/${LEVI_MOD_ID}")
  set(LEVI_PACK_FILE "${CMAKE_CURRENT_BINARY_DIR}/Levi-Distant-Horizon-${LEVI_MOD_VERSION}-arm64-v8a.levipack")

  configure_file(manifest.json.in "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" @ONLY)

  add_custom_target(levi_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LEVI_PACKAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LEVI_MOD_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:levi_distant_horizon>" "${LEVI_MOD_DIR}/liblevi_distant_horizon.so"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" "${LEVI_MOD_DIR}/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${LEVI_PACKAGE_DIR}" "${CMAKE_COMMAND}" -E tar cf "${LEVI_PACK_FILE}" --format=zip "${LEVI_MOD_ID}"
    DEPENDS levi_distant_horizon
    COMMENT "Creating LeviLauncher .levipack"
    VERBATIM
  )
endif()
