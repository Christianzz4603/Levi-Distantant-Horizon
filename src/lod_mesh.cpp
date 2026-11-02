#include "ldh/lod_mesh.hpp"

#include <cmath>
#include <limits>

namespace ldh {
namespace {

[[nodiscard]] bool multiply_fits_size(std::size_t a, std::size_t b) noexcept {
    return b == 0U || a <= std::numeric_limits<std::size_t>::max() / b;
}

} // namespace

MeshResult build_lod_mesh(const HeightField& field, std::uint32_t sample_step) {
    MeshResult result{};

    if (field.width < 2U || field.depth < 2U) {
        result.error = MeshError::InvalidDimensions;
        return result;
    }

    if (!std::isfinite(field.cell_size) || field.cell_size <= 0.0F) {
        result.error = MeshError::InvalidCellSize;
        return result;
    }

    if (sample_step == 0U) {
        result.error = MeshError::InvalidSampleStep;
        return result;
    }

    const auto expected = static_cast<std::size_t>(field.width) *
                          static_cast<std::size_t>(field.depth);
    if (!multiply_fits_size(field.width, field.depth) ||
        field.heights.size() != expected) {
        result.error = MeshError::HeightCountMismatch;
        return result;
    }

    const std::uint32_t x_cells = field.width - 1U;
    const std::uint32_t z_cells = field.depth - 1U;
    const std::uint32_t x_segments = (x_cells + sample_step - 1U) / sample_step;
    const std::uint32_t z_segments = (z_cells + sample_step - 1U) / sample_step;

    const std::size_t vertex_count =
        (static_cast<std::size_t>(x_segments) + 1U) *
        (static_cast<std::size_t>(z_segments) + 1U);

    if (vertex_count > static_cast<std::size_t>(
                           std::numeric_limits<std::uint32_t>::max())) {
        result.error = MeshError::TooManyVertices;
        return result;
    }

    const std::size_t index_count =
        static_cast<std::size_t>(x_segments) *
        static_cast<std::size_t>(z_segments) * 6U;

    result.mesh.vertices.reserve(vertex_count);
    result.mesh.indices.reserve(index_count);

    for (std::uint32_t z = 0U; z <= z_segments; ++z) {
        const std::uint32_t source_z =
            (z == z_segments) ? z_cells : z * sample_step;

        for (std::uint32_t x = 0U; x <= x_segments; ++x) {
            const std::uint32_t source_x =
                (x == x_segments) ? x_cells : x * sample_step;
            const std::size_t source_index =
                static_cast<std::size_t>(source_z) * field.width + source_x;
            const float height = field.heights[source_index];

            if (!std::isfinite(height)) {
                result.mesh = {};
                result.error = MeshError::HeightCountMismatch;
                return result;
            }

            result.mesh.vertices.push_back(Vertex{
                static_cast<float>(source_x) * field.cell_size,
                height,
                static_cast<float>(source_z) * field.cell_size
            });
        }
    }

    const std::uint32_t row_width = x_segments + 1U;
    for (std::uint32_t z = 0U; z < z_segments; ++z) {
        for (std::uint32_t x = 0U; x < x_segments; ++x) {
            const std::uint32_t top_left = z * row_width + x;
            const std::uint32_t top_right = top_left + 1U;
            const std::uint32_t bottom_left = top_left + row_width;
            const std::uint32_t bottom_right = bottom_left + 1U;

            result.mesh.indices.insert(result.mesh.indices.end(), {
                top_left, bottom_left, top_right,
                top_right, bottom_left, bottom_right
            });
        }
    }

    return result;
}

} // namespace ldh
