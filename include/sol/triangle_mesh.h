#ifndef SOL_TRIANGLE_MESH_H
#define SOL_TRIANGLE_MESH_H

#include <tuple>
#include <memory>

#include <proto/triangle.h>

#include "sol/geometry.h"

namespace sol {

class Light;

/// Triangle mesh with an underlying acceleration data structure to speed up intersection tests.
class TriangleMesh : public Geometry {
public:
    TriangleMesh(
        std::vector<size_t>&& indices,
        std::vector<proto::Vec3f>&& vertices,
        std::vector<proto::Vec3f>&& normals,
        std::vector<proto::Vec2f>&& tex_coords,
        std::vector<const Bsdf*>&& bsdfs,
        std::unordered_map<size_t, const Light*>&& lights);
    ~TriangleMesh();

    std::optional<Hit> intersect_closest(proto::Rayf&) const override;
    bool intersect_any(const proto::Rayf&) const override;

    /// Returns the number of triangles in the mesh.
    size_t triangle_count() const { return indices_.size() / 3; }

    /// Returns the vertex indices of a triangle located a given triangle index.
    std::tuple<size_t, size_t, size_t> triangle_indices(size_t triangle_index) const {
        return std::tuple {
            indices_[triangle_index * 3 + 0],
            indices_[triangle_index * 3 + 1],
            indices_[triangle_index * 3 + 2]
        };
    }

    const std::vector<proto::Vec3f>& normals()    const { return normals_; }
    const std::vector<proto::Vec2f>& tex_coords() const { return tex_coords_; }
    const std::vector<const Bsdf*>&  bsdfs()      const { return bsdfs_; }

private:
    struct BvhData;
    template <typename Executor>
    std::unique_ptr<BvhData> build_bvh(Executor&, const std::vector<proto::Vec3f>&) const;
    template <typename Executor>
    std::vector<proto::PrecomputedTrianglef> build_triangles(Executor&, const std::vector<proto::Vec3f>&) const;

    std::vector<size_t> indices_;
    std::vector<proto::PrecomputedTrianglef> triangles_;
    std::vector<proto::Vec3f> normals_;
    std::vector<proto::Vec2f> tex_coords_;
    std::vector<const Bsdf*> bsdfs_;
    std::unordered_map<size_t, const Light*> lights_;
    std::unique_ptr<BvhData> bvh_data_;
};

} // namespace sol

#endif
