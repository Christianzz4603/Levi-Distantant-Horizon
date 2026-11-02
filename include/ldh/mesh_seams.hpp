#pragma once
#include "ldh/lod_mesh.hpp"
namespace ldh {
// Adds downward-facing boundary skirts to a row-major heightfield mesh.
// grid_width/grid_depth must describe the original, skirt-free vertex grid.
[[nodiscard]] MeshError append_edge_skirts(Mesh& mesh, std::uint32_t grid_width,
                                           std::uint32_t grid_depth,
                                           float skirt_depth) noexcept;
} // namespace ldh
