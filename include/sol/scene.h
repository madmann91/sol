#ifndef SOL_SCENE_H
#define SOL_SCENE_H

#include <memory>
#include <vector>
#include <optional>

#include <proto/ray.h>

#include "sol/bsdfs.h"
#include "sol/lights.h"

namespace sol {

class Texture;
class Camera;
struct Material;

/// Result of intersecting a ray with a scene node.
struct Hit {
    SurfaceInfo surf_info;  ///< Surface information at the hit point
    const Light* light;     ///< Light source at the hit point, if any (can be null)
    const Bsdf* bsdf;       ///< BSDF at the hit point, if any (can be null)
};

struct Scene {
    template <typename T> using unique_vector = std::vector<std::unique_ptr<T>>;

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

    std::optional<Hit> intersect_closest(proto::Rayf& ray) const { return root_node->intersect_closest(ray); }
    bool intersect_any(const proto::Rayf& ray) const { return root_node->intersect_any(ray); }

    std::unique_ptr<Node>   root_node;
    std::unique_ptr<Camera> camera;

    unique_vector<Bsdf>     bsdfs;
    unique_vector<Light>    lights;
    unique_vector<Texture>  textures;
    unique_vector<Material> materials;
};

} // namespace sol

#endif
