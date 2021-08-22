#include <ranges>
#include <numeric>

#include <proto/triangle.h>

#include <bvh/bvh.h>
#include <bvh/sweep_sah_builder.h>
#if defined(SOL_ENABLE_TBB)
#include <bvh/tbb/parallel_top_down_scheduler.h>
#include <par/tbb/executors.h>
#elif defined(SOL_ENABLE_OMP)
#include <bvh/omp/parallel_top_down_scheduler.h>
#include <par/omp/executors.h>
#else
#include <bvh/sequential_top_down_scheduler.h>
#include <par/sequential_executor.h>
#endif
#include <bvh/topology_modifier.h>
#include <bvh/sequential_reinsertion_optimizer.h>
#include <bvh/single_ray_traverser.h>

#include "sol/triangle_mesh.h"
#include "sol/lights.h"

namespace sol {

using Bvh = bvh::Bvh<float>;
struct TriangleMesh::BvhData { Bvh bvh; };

#if defined(SOL_ENABLE_TBB)
template <typename Builder>
using TopDownScheduler = bvh::tbb::ParallelTopDownScheduler<Builder>;
using Executor         = par::tbb::Executor;
#elif defined(SOL_ENABLE_OMP)
template <typename Builder>
using TopDownScheduler = bvh::omp::ParallelTopDownScheduler<Builder>;
using Executor         = par::omp::StaticExecutor;
#else
template <typename Builder>
using TopDownScheduler = bvh::SequentialTopDownScheduler<Builder>;
using Executor         = par::SequentialExecutor;
#endif

TriangleMesh::TriangleMesh(
    std::vector<size_t>&& indices,
    std::vector<proto::Vec3f>&& vertices,
    std::vector<proto::Vec3f>&& normals,
    std::vector<proto::Vec2f>&& tex_coords,
    std::vector<const Bsdf*>&& bsdfs,
    std::unordered_map<size_t, const Light*>&& lights)
    : indices_(std::move(indices))
    , normals_(std::move(normals))
    , tex_coords_(std::move(tex_coords))
    , bsdfs_(std::move(bsdfs))
    , lights_(std::move(lights))
{
    Executor executor;
    bvh_data_ = build_bvh(executor, vertices);
    triangles_ = build_triangles(executor, vertices);
}

TriangleMesh::~TriangleMesh() = default;

std::optional<Hit> TriangleMesh::intersect_closest(proto::Rayf& ray) const {
    auto hit_info = bvh::SingleRayTraverser<Bvh>::traverse<false>(ray, bvh_data_->bvh,
        [&] (proto::Rayf& ray, const Bvh::Node& leaf) {
            std::optional<std::tuple<size_t, float, float>> hit_info;
            for (size_t i = leaf.first_index, n = i + leaf.prim_count; i < n; ++i) {
                if (auto uv = triangles_[i].intersect(ray))
                    hit_info = std::make_optional(std::tuple { i, uv->first, uv->second });
            }
            return hit_info;
        });

    if (!hit_info)
        return std::nullopt;

    auto [permuted_index, u, v] = *hit_info;
    auto face_normal    = triangles_[permuted_index].normal();
    auto triangle_index = bvh_data_->bvh.prim_indices[permuted_index];
    auto [i0, i1, i2]   = triangle_indices(triangle_index);

    auto normal     = proto::lerp(normals_[i0], normals_[i1], normals_[i2], u, v);
    auto tex_coords = proto::lerp(tex_coords_[i0], tex_coords_[i1], tex_coords_[i2], u, v);

    // Flip normals based on the side of the triangle
    bool is_front_side = proto::dot(face_normal, ray.dir) < 0;
    if (!is_front_side) {
        face_normal = -face_normal;
        normal = -normal;
    }

    SurfaceInfo surf_info;
    surf_info.is_front_side = is_front_side;
    surf_info.point         = ray.point_at(ray.tmax);
    surf_info.tex_coords    = tex_coords;
    surf_info.surf_coords   = proto::Vec2f(u, v);
    surf_info.face_normal   = face_normal;
    surf_info.local         = proto::ortho_basis(proto::normalize(normal));

    const Light* light = nullptr;
    if (auto it = lights_.find(triangle_index); it != lights_.end())
        light = it->second;
    return std::make_optional(Hit { surf_info, light, bsdfs_[triangle_index] });
}

bool TriangleMesh::intersect_any(const proto::Rayf& init_ray) const {
    auto ray = init_ray;
    return bvh::SingleRayTraverser<Bvh>::traverse<true>(ray, bvh_data_->bvh,
        [&] (proto::Rayf& ray, const Bvh::Node& leaf) {
            for (size_t i = leaf.first_index, n = i + leaf.prim_count; i < n; ++i) {
                if (triangles_[i].intersect(ray))
                    return true;
            }
            return false;
        });
}

template <typename Executor>
std::unique_ptr<TriangleMesh::BvhData> TriangleMesh::build_bvh(Executor& executor, const std::vector<proto::Vec3f>& vertices) const {
    using Builder = bvh::SweepSahBuilder<Bvh>;

    TopDownScheduler<Builder> top_down_scheduler;

    auto bboxes  = std::make_unique<proto::BBoxf[]>(triangle_count());
    auto centers = std::make_unique<proto::Vec3f[]>(triangle_count());

    // Compute bounding boxes and centers in parallel
    auto global_bbox = par::transform_reduce(
        executor, par::range_1d(size_t{0}, triangle_count()), proto::BBoxf::empty(),
        [] (proto::BBoxf left, const proto::BBoxf& right) { return left.extend(right); },
        [&] (size_t i) -> proto::BBoxf {
            auto triangle = proto::Trianglef(
                vertices[indices_[i * 3 + 0]],
                vertices[indices_[i * 3 + 1]],
                vertices[indices_[i * 3 + 2]]);
            auto bbox  = triangle.bbox();
            centers[i] = triangle.center();
            return bboxes[i] = bbox;
        });

    auto bvh = Builder::build(top_down_scheduler, executor, global_bbox, bboxes.get(), centers.get(), triangle_count());
    bvh::TopologyModifier topo_modifier(bvh, bvh.parents(executor));
    bvh::SequentialReinsertionOptimizer<Bvh>::optimize(topo_modifier);
    return std::make_unique<BvhData>(BvhData { std::move(bvh) });
}

template <typename Executor>
std::vector<proto::PrecomputedTrianglef> TriangleMesh::build_triangles(Executor& executor, const std::vector<proto::Vec3f>& vertices) const {
    // Build a permuted array of triangles, so as to avoid indirections when intersecting the mesh with a ray.
    std::vector<proto::PrecomputedTrianglef> triangles(triangle_count());
    par::for_each(executor, par::range_1d(size_t{0}, triangle_count()), [&] (size_t i) {
        auto j = bvh_data_->bvh.prim_indices[i];
        triangles[i] = proto::PrecomputedTrianglef(
            vertices[indices_[j * 3 + 0]],
            vertices[indices_[j * 3 + 1]],
            vertices[indices_[j * 3 + 2]]);
    });
    return triangles;
}

} // namespace sol
