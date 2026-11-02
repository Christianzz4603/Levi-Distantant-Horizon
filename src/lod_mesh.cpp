#include "ldh/lod_mesh.hpp"
#include <cmath>
#include <limits>
#include <new>
namespace ldh { namespace {
bool fits(std::size_t a, std::size_t b) noexcept { return b == 0U || a <= std::numeric_limits<std::size_t>::max() / b; }
bool cancelled(const std::atomic_bool* flag) noexcept { return flag && flag->load(std::memory_order_relaxed); }
}
MeshResult build_lod_mesh(const HeightField& field, std::uint32_t step, const std::atomic_bool* cancel_flag) {
    MeshResult out{};
    if (cancelled(cancel_flag)) { out.error = MeshError::Cancelled; return out; }
    if (field.width < 2U || field.depth < 2U) { out.error = MeshError::InvalidDimensions; return out; }
    if (!std::isfinite(field.cell_size) || field.cell_size <= 0.0F) { out.error = MeshError::InvalidCellSize; return out; }
    if (step == 0U) { out.error = MeshError::InvalidSampleStep; return out; }
    const std::size_t w = field.width, d = field.depth;
    if (!fits(w, d) || field.heights.size() != w * d) { out.error = MeshError::HeightCountMismatch; return out; }
    const std::uint32_t xc = field.width - 1U, zc = field.depth - 1U;
    const std::uint32_t xs = 1U + (xc - 1U) / step, zs = 1U + (zc - 1U) / step;
    const std::size_t cols = static_cast<std::size_t>(xs) + 1U, rows = static_cast<std::size_t>(zs) + 1U;
    if (!fits(cols, rows)) { out.error = MeshError::TooManyVertices; return out; }
    const std::size_t vertices = cols * rows;
    std::size_t cells = 0U;
    if (vertices > std::numeric_limits<std::uint32_t>::max() || !fits(xs, zs)) { out.error = MeshError::TooManyVertices; return out; }
    cells = static_cast<std::size_t>(xs) * zs;
    if (!fits(cells, 6U)) { out.error = MeshError::TooManyVertices; return out; }
    try {
        out.mesh.vertices.reserve(vertices);
        out.mesh.indices.reserve(cells * 6U);
        for (std::uint32_t z = 0U; z <= zs; ++z) {
            if (cancelled(cancel_flag)) { out.mesh = {}; out.error = MeshError::Cancelled; return out; }
            const std::uint32_t sz = z == zs ? zc : z * step;
            for (std::uint32_t x = 0U; x <= xs; ++x) {
                const std::uint32_t sx = x == xs ? xc : x * step;
                const float h = field.heights[static_cast<std::size_t>(sz) * w + sx];
                if (!std::isfinite(h)) { out.mesh = {}; out.error = MeshError::HeightCountMismatch; return out; }
                out.mesh.vertices.push_back({static_cast<float>(sx) * field.cell_size, h, static_cast<float>(sz) * field.cell_size});
            }
        }
        const std::uint32_t row_width = xs + 1U;
        for (std::uint32_t z = 0U; z < zs; ++z) {
            if (cancelled(cancel_flag)) { out.mesh = {}; out.error = MeshError::Cancelled; return out; }
            for (std::uint32_t x = 0U; x < xs; ++x) {
                const std::uint32_t tl = z * row_width + x, tr = tl + 1U, bl = tl + row_width, br = bl + 1U;
                out.mesh.indices.insert(out.mesh.indices.end(), {tl, bl, tr, tr, bl, br});
            }
        }
    } catch (const std::bad_alloc&) { out.mesh = {}; out.error = MeshError::AllocationFailed; }
    return out;
}
} // namespace ldh
