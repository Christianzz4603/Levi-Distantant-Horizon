#include "ldh/mesh_seams.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
namespace {
ldh::Mesh grid() {
    ldh::Mesh mesh;
    for (std::uint32_t z = 0; z < 3U; ++z)
        for (std::uint32_t x = 0; x < 3U; ++x)
            mesh.vertices.push_back({static_cast<float>(x), 10.0F, static_cast<float>(z)});
    mesh.indices = {0U, 3U, 1U, 1U, 3U, 4U, 1U, 4U, 2U};
    return mesh;
}
void creates_all_four_boundary_skirts() {
    auto mesh = grid();
    assert(ldh::append_edge_skirts(mesh, 3U, 3U, 4.0F) == ldh::MeshError::None);
    assert(mesh.vertices.size() == 21U);
    assert(mesh.indices.size() == 57U);
    for (std::size_t i = 9U; i < mesh.vertices.size(); ++i)
        assert(std::abs(mesh.vertices[i].y - 6.0F) < 0.001F);
    for (const auto index : mesh.indices) assert(index < mesh.vertices.size());
}
void invalid_input_does_not_mutate_mesh() {
    auto mesh = grid();
    const auto old_vertices = mesh.vertices.size();
    const auto old_indices = mesh.indices.size();
    assert(ldh::append_edge_skirts(mesh, 3U, 3U, 0.0F) == ldh::MeshError::InvalidSkirtDepth);
    assert(ldh::append_edge_skirts(mesh, 4U, 3U, 2.0F) == ldh::MeshError::InvalidMesh);
    assert(mesh.vertices.size() == old_vertices && mesh.indices.size() == old_indices);
}
}
int main() {
    creates_all_four_boundary_skirts(); invalid_input_does_not_mutate_mesh();
    std::cout << "All mesh seam tests passed.\n";
}
