#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ldh {

struct HeightField {
    std::uint32_t width{};
    std::uint32_t depth{};
    float cell_size{1.0F};
    std::vector<float> heights;
};

struct Vertex {
    float x{};
    float y{};
    float z{};
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};

enum class MeshError {
    None,
    InvalidDimensions,
    InvalidCellSize,
    HeightCountMismatch,
    InvalidSampleStep,
    TooManyVertices
};

struct MeshResult {
    Mesh mesh;
    MeshError error{MeshError::None};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == MeshError::None;
    }
};

// Builds a simplified grid mesh by sampling every sample_step cells.
// The input height field is expected to be row-major: z * width + x.
[[nodiscard]] MeshResult build_lod_mesh(
    const HeightField& field,
    std::uint32_t sample_step
);

} // namespace ldh
