#include <ranges>
#include <numeric>
#include <execution>

#include <proto/triangle.h>

#include <bvh/sweep_sah_builder.h>
#ifdef BVH_ENABLE_TBB
#include <bvh/parallel_top_down_scheduler.h>
#else
#include <bvh/sequential_top_down_scheduler.h>
#endif
#include <bvh/sequential_reinsertion_optimizer.h>
#include <bvh/single_ray_traverser.h>

#include "sol/triangle_mesh.h"
#include "sol/lights.h"

namespace sol {

#ifdef BVH_ENABLE_TBB
template <typename Builder>
using TopDownScheduler = bvh::ParallelTopDownScheduler<Builder>;
#else
template <typename Builder>
using TopDownScheduler = bvh::SequentialTopDownBuilder<Builder>;
#endif

TriangleMesh::TriangleMesh(
    std::vector<size_t>&& indices,
    std::vector<proto::Vec3f>&& vertices,
    std::vector<proto::Vec3f>&& normals,
    std::vector<proto::Vec2f>&& tex_coords,
    std::vector<const Bsdf*>&& bsdfs,
    std::unordered_map<size_t, const TriangleLight*> lights)
    : indices_(std::move(indices))
    , vertices_(std::move(vertices))
    , normals_(std::move(normals))
    , tex_coords_(std::move(tex_coords))
    , bsdfs_(std::move(bsdfs))
    , lights_(std::move(lights))
{
    bvh_ = build_bvh();
}

std::optional<Hit> TriangleMesh::intersect_closest(proto::Rayf& ray) const {
    using HitInfo = std::tuple<size_t, proto::Vec3f, float, float>;
    auto hit_info = bvh::SingleRayTraverser<Bvh>::traverse<false>(ray, bvh_,
        [&] (proto::Rayf& ray, const Bvh::Node& leaf) {
            std::optional<HitInfo> hit_info;
            for (size_t i = 0; i < leaf.prim_count; ++i) {
                auto triangle_index = bvh_.prim_indices[leaf.first_index + i];
                auto triangle = triangle_at(triangle_index);
                if (auto uv = triangle.intersect(ray))
                    hit_info = std::make_optional(HitInfo { triangle_index, triangle.normal(), uv->first, uv->second });
            }
            return hit_info;
        });

    if (!hit_info)
        return std::nullopt;

    auto [triangle_index, face_normal, u, v] = *hit_info;
    auto [i0, i1, i2] = triangle_indices(triangle_index);
    auto normal     = proto::lerp(normals_[i0], normals_[i1], normals_[i2], u, v);
    auto tex_coords = proto::lerp(tex_coords_[i0], tex_coords_[i1], tex_coords_[i2], u, v);

    SurfaceInfo surf_info;
    surf_info.is_front_side = proto::dot(face_normal, ray.dir) < 0;
    surf_info.point         = ray.point_at(ray.tmax);
    surf_info.tex_coords    = tex_coords;
    surf_info.surf_coords   = proto::Vec2f(u, v);
    surf_info.face_normal   = proto::normalize(face_normal);
    surf_info.local         = proto::ortho_basis(proto::normalize(normal));

    const Light* light = nullptr;
    if (auto it = lights_.find(triangle_index); it != lights_.end())
        light = it->second;
    return std::make_optional(Hit { surf_info, light, bsdfs_[triangle_index] });
}

bool TriangleMesh::intersect_any(const proto::Rayf& init_ray) const {
    auto ray = init_ray;
    return static_cast<bool>(bvh::SingleRayTraverser<Bvh>::traverse<true>(ray, bvh_,
        [&] (proto::Rayf& ray, const Bvh::Node& leaf) -> std::optional<std::tuple<>> {
            for (size_t i = 0; i < leaf.prim_count; ++i) {
                if (triangle_at(bvh_.prim_indices[leaf.first_index + i]).intersect(ray))
                    return std::make_optional(std::make_tuple());
            }
            return std::nullopt;
        }));
}

TriangleMesh::Bvh TriangleMesh::build_bvh() const {
    auto bboxes  = std::make_unique<proto::BBoxf[]>(triangle_count());
    auto centers = std::make_unique<proto::Vec3f[]>(triangle_count());

    // Compute bounding boxes and centers in parallel
    auto range = std::views::iota(size_t{0}, triangle_count());
    auto global_bbox = std::transform_reduce(
        std::execution::par_unseq,
        range.begin(), range.end(), proto::BBoxf::empty(),
        [] (proto::BBoxf left, const proto::BBoxf& right) { return left.extend(right); },
        [&] (size_t i) {
            auto triangle = triangle_at(i);
            auto bbox = triangle.bbox();
            bboxes[i]  = bbox;
            centers[i] = triangle.center();
            return bbox;
        });

    using Builder = bvh::SweepSahBuilder<Bvh>;
    TopDownScheduler<Builder> scheduler;
    auto bvh = Builder::build(scheduler, global_bbox, bboxes.get(), centers.get(), triangle_count());
    bvh::SequentialReinsertionOptimizer<Bvh>::optimize(bvh);
    return bvh;
}

} // namespace sol
