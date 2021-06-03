#ifndef SOL_TRIANGLE_MESH_H
#define SOL_TRIANGLE_MESH_H

#include <tuple>

#include <proto/triangle.h>
#include <bvh/bvh.h>

#include "sol/scene.h"

namespace sol {

class TriangleLight;

/// Triangle mesh with an underlying acceleration data structure to speed up intersection tests.
class TriangleMesh : public Scene::Node {
public:
    TriangleMesh(
        std::vector<size_t>&& indices,
        std::vector<proto::Vec3f>&& vertices,
        std::vector<proto::Vec3f>&& normals,
        std::vector<proto::Vec2f>&& texcoords,
        std::vector<const Bsdf*>&& bsdfs,
        std::unordered_map<size_t, const TriangleLight*> lights);

    std::optional<Hit> intersect(proto::Rayf&) const override;
    bool is_occluded(const proto::Rayf&) const override;

    /// Returns the number of triangles in the mesh.
    size_t triangle_count() const { return indices_.size() / 3; }

    /// Returns the triangle located at the given triangle index.
    proto::Trianglef triangle_at(size_t triangle_index) const {
        auto [i0, i1, i2] = triangle_indices(triangle_index);
        return proto::Trianglef(vertices_[i0], vertices_[i1], vertices_[i2]);
    }

    /// Returns the vertex indices of a triangle located a given triangle index.
    std::tuple<size_t, size_t, size_t> triangle_indices(size_t triangle_index) const {
        return std::tuple {
            indices_[triangle_index * 3 + 0],
            indices_[triangle_index * 3 + 1],
            indices_[triangle_index * 3 + 2]
        };
    }

    const std::vector<proto::Vec3f>& vertices()  const { return vertices_;  }
    const std::vector<proto::Vec3f>& normals()   const { return normals_; }
    const std::vector<proto::Vec2f>& texcoords() const { return texcoords_; }

    const std::vector<const Bsdf*>& bsdfs() const { return bsdfs_; }

private:
    using Bvh = bvh::Bvh<float>;
    Bvh build_bvh() const;

    std::vector<size_t> indices_;
    std::vector<proto::Vec3f> vertices_;
    std::vector<proto::Vec3f> normals_;
    std::vector<proto::Vec2f> texcoords_;
    std::vector<const Bsdf*> bsdfs_;
    std::unordered_map<size_t, const TriangleLight*> lights_;
    Bvh bvh_;
};

} // namespace sol

#endif
