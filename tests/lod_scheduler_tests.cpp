#include "ldh/lod_scheduler.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <utility>
namespace {
ldh::HeightField flat(std::uint32_t w, std::uint32_t d) {
    ldh::HeightField f{}; f.width=w; f.depth=d; f.heights.resize(static_cast<std::size_t>(w)*d, 4.0F); return f;
}
void async_mesh() {
    ldh::LodScheduler s({1U,4U,1024U}); ldh::LodJob j{}; j.id=1U; j.field=flat(8U,8U); j.sample_step=2U;
    auto f=s.submit(std::move(j)); assert(f.wait_for(std::chrono::seconds(3))==std::future_status::ready);
    const auto r=f.get(); assert(r.scheduler_error==ldh::SchedulerError::None); assert(r.mesh_result);
    assert(r.mesh_result.mesh.vertices.size()==25U); assert(r.mesh_result.mesh.indices.size()==96U);
}
void cancellation() {
    ldh::LodScheduler s({1U,2U,2'000'000U}); ldh::LodJob j{}; j.id=2U; j.field=flat(1024U,1024U);
    auto f=s.submit(std::move(j)); assert(s.cancel(2U));
    assert(f.wait_for(std::chrono::seconds(5))==std::future_status::ready);
    const auto r=f.get(); assert(r.scheduler_error==ldh::SchedulerError::Cancelled || r.mesh_result.error==ldh::MeshError::Cancelled);
    assert(r.mesh_result.mesh.vertices.empty());
}
void input_budget() {
    ldh::LodScheduler s({1U,2U,16U}); ldh::LodJob j{}; j.id=3U; j.field=flat(8U,8U);
    assert(s.submit(std::move(j)).get().scheduler_error==ldh::SchedulerError::InputTooLarge);
}
void worker_cap() { ldh::LodScheduler s({64U,4U,1024U}); assert(s.worker_count()==8U); }
}
int main() { async_mesh(); cancellation(); input_budget(); worker_cap(); std::cout << "All Levi Distant Horizon scheduler tests passed.\n"; }
