#ifndef SOL_SCENE_H
#define SOL_SCENE_H

#include <memory>
#include <vector>
#include <optional>

#include <proto/mat.h>
#include <proto/vec.h>
#include <proto/ray.h>

namespace sol {

class Bsdf;
class Light;
class Texture;
class Camera;
class Image;

/// Surface information for a specific point on a surface.
/// This information is required to perform various shading operations.
struct SurfaceInfo {
    bool is_front_side;         ///< True if the point is on the front of the surface
    proto::Vec3f point;         ///< Hit point in world coordinates
    proto::Vec2f tex_coords;    ///< Texture coordinates
    proto::Vec2f surf_coords;   ///< Coordinates on the surface (depends on the surface type)
    proto::Vec3f face_normal;   ///< Geometric normal
    proto::Mat3x3f local;       ///< Local coordinates at the hit point, w.r.t shading normal

    proto::Vec3f normal() const { return local.col(2); }
};

/// Result of intersecting a ray with a scene node.
struct Hit {
    SurfaceInfo surf_info;  ///< Surface information at the hit point
    const Light* light;     ///< Light source at the hit point, if any (can be null)
    const Bsdf* bsdf;       ///< BSDF at the hit point, if any (can be null)
};

/// Owning collection of lights, BSDFs, textures and nodes that make up a scene.
struct Scene {
    class Node {
    public:
        virtual ~Node() {}

        /// Intersects the node with a ray, returns either a `Hit` that corresponds
        /// to the closest intersection along the ray, or nothing.
        /// If an intersection is found, the `tmax` parameter of the ray is updated.
        virtual std::optional<Hit> intersect_closest(proto::Rayf&) const = 0;
        /// Tests if a given ray intersects the node or not.
        virtual bool intersect_any(const proto::Rayf&) const = 0;
    };

    Scene();
    ~Scene();

    std::optional<Hit> intersect_closest(proto::Rayf& ray) const { return root_node->intersect_closest(ray); }
    bool intersect_any(const proto::Rayf& ray) const { return root_node->intersect_any(ray); }

    std::unique_ptr<Node>   root_node;
    std::unique_ptr<Camera> camera;

    template <typename T> using unique_vector = std::vector<std::unique_ptr<T>>;

    unique_vector<Bsdf>    bsdfs;
    unique_vector<Light>   lights;
    unique_vector<Texture> textures;
    unique_vector<Image>   images;

    /// Loads the given scene file.
    bool load(const std::string_view& file_name);
};

} // namespace sol

#endif
