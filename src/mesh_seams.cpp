#include "ldh/mesh_seams.hpp"
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include <vector>
namespace ldh {
MeshError append_edge_skirts(Mesh& mesh, std::uint32_t grid_width,
                             std::uint32_t grid_depth, float skirt_depth) noexcept {
    if (grid_width < 2U || grid_depth < 2U) return MeshError::InvalidDimensions;
    if (!std::isfinite(skirt_depth) || skirt_depth <= 0.0F) return MeshError::InvalidSkirtDepth;
    const auto width = static_cast<std::size_t>(grid_width);
    const auto depth = static_cast<std::size_t>(grid_depth);
    if (width > std::numeric_limits<std::size_t>::max() / depth) return MeshError::InvalidMesh;
    const auto original_count = width * depth;
    if (mesh.vertices.size() != original_count) return MeshError::InvalidMesh;
    for (const auto& vertex : mesh.vertices) {
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z) ||
            !std::isfinite(vertex.y - skirt_depth)) return MeshError::InvalidMesh;
    }
    for (const auto index : mesh.indices)
        if (index >= original_count) return MeshError::InvalidMesh;

    const auto skirt_vertices = 2U * (width + depth);
    const auto segments = 2U * (width - 1U) + 2U * (depth - 1U);
    if (skirt_vertices > std::numeric_limits<std::uint32_t>::max() - original_count ||
        segments > (std::numeric_limits<std::size_t>::max() - mesh.indices.size()) / 6U)
        return MeshError::TooManyVertices;

    try {
        Mesh result = mesh;
        result.vertices.reserve(original_count + skirt_vertices);
        result.indices.reserve(mesh.indices.size() + segments * 6U);

        const auto add_edge = [&result, skirt_depth](const std::vector<std::uint32_t>& edge,
                                                     bool reverse_winding) {
            const auto lower_start = static_cast<std::uint32_t>(result.vertices.size());
            for (const auto index : edge) {
                const auto& source = result.vertices[index];
                result.vertices.push_back({source.x, source.y - skirt_depth, source.z});
            }
            for (std::size_t i = 0; i + 1U < edge.size(); ++i) {
                const auto a = edge[i];
                const auto b = edge[i + 1U];
                const auto lower_a = lower_start + static_cast<std::uint32_t>(i);
                const auto lower_b = lower_a + 1U;
                if (!reverse_winding) {
                    result.indices.insert(result.indices.end(), {a, b, lower_a, b, lower_b, lower_a});
                } else {
                    result.indices.insert(result.indices.end(), {a, lower_a, b, b, lower_a, lower_b});
                }
            }
        };

        std::vector<std::uint32_t> edge;
        edge.reserve(std::max(width, depth));
        for (std::uint32_t x = 0U; x < grid_width; ++x) edge.push_back(x);
        add_edge(edge, false);

        edge.clear();
        for (std::uint32_t z = 0U; z < grid_depth; ++z)
            edge.push_back(z * grid_width + (grid_width - 1U));
        add_edge(edge, false);

        edge.clear();
        const auto bottom = (grid_depth - 1U) * grid_width;
        for (std::uint32_t x = 0U; x < grid_width; ++x) edge.push_back(bottom + x);
        add_edge(edge, true);

        edge.clear();
        for (std::uint32_t z = 0U; z < grid_depth; ++z) edge.push_back(z * grid_width);
        add_edge(edge, true);

        mesh = std::move(result);
        return MeshError::None;
    } catch (const std::bad_alloc&) {
        return MeshError::AllocationFailed;
    } catch (...) {
        return MeshError::AllocationFailed;
    }
}
} // namespace ldh
