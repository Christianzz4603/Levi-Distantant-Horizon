#include "ldh/lod_mesh.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

ldh::HeightField flat_field(std::uint32_t width, std::uint32_t depth) {
    ldh::HeightField field{};
    field.width = width;
    field.depth = depth;
    field.cell_size = 1.0F;
    field.heights.resize(static_cast<std::size_t>(width) * depth, 0.0F);
    return field;
}

void test_full_resolution_mesh() {
    const auto field = flat_field(3U, 3U);
    const auto result = ldh::build_lod_mesh(field, 1U);
    assert(result);
    assert(result.mesh.vertices.size() == 9U);
    assert(result.mesh.indices.size() == 24U);
}

void test_simplified_mesh_keeps_edge() {
    auto field = flat_field(6U, 6U);
    field.cell_size = 2.0F;
    const auto result = ldh::build_lod_mesh(field, 2U);
    assert(result);
    assert(result.mesh.vertices.size() == 16U);
    assert(result.mesh.indices.size() == 54U);
    assert(std::fabs(result.mesh.vertices.back().x - 10.0F) < 0.001F);
    assert(std::fabs(result.mesh.vertices.back().z - 10.0F) < 0.001F);
}

void test_invalid_inputs() {
    auto field = flat_field(1U, 4U);
    assert(ldh::build_lod_mesh(field, 1U).error == ldh::MeshError::InvalidDimensions);

    field = flat_field(4U, 4U);
    assert(ldh::build_lod_mesh(field, 0U).error == ldh::MeshError::InvalidSampleStep);

    field.cell_size = 0.0F;
    assert(ldh::build_lod_mesh(field, 1U).error == ldh::MeshError::InvalidCellSize);

    field = flat_field(4U, 4U);
    field.heights.pop_back();
    assert(ldh::build_lod_mesh(field, 1U).error == ldh::MeshError::HeightCountMismatch);
}

void test_non_finite_height_rejected() {
    auto field = flat_field(2U, 2U);
    field.heights[2] = std::numeric_limits<float>::quiet_NaN();
    const auto result = ldh::build_lod_mesh(field, 1U);
    assert(!result);
    assert(result.mesh.vertices.empty());
}

} // namespace

int main() {
    test_full_resolution_mesh();
    test_simplified_mesh_keeps_edge();
    test_invalid_inputs();
    test_non_finite_height_rejected();
    std::cout << "All Levi Distant Horizon core tests passed.\n";
}
