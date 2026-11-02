#pragma once
#include <atomic>
#include <cstdint>
#include <vector>
namespace ldh {
struct HeightField { std::uint32_t width{}, depth{}; float cell_size{1.0F}; std::vector<float> heights; };
struct Vertex { float x{}, y{}, z{}; };
struct Mesh { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
enum class MeshError { None, InvalidDimensions, InvalidCellSize, HeightCountMismatch, InvalidSampleStep, TooManyVertices, Cancelled, AllocationFailed, InvalidMesh, InvalidSkirtDepth };
struct MeshResult {
    Mesh mesh;
    MeshError error{MeshError::None};
    [[nodiscard]] explicit operator bool() const noexcept { return error == MeshError::None; }
};
[[nodiscard]] MeshResult build_lod_mesh(const HeightField& field, std::uint32_t sample_step, const std::atomic_bool* cancel_flag = nullptr);
} // namespace ldh
