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
struct Material;

/// Result of intersecting a ray with a scene node.
struct Hit {
    SurfaceInfo surf_info;
    std::optional<EmissionValue> emission;
    const Bsdf* bsdf;
};

struct Scene {
    template <typename T> using unique_vector = std::vector<std::unique_ptr<T>>;

    class Node {
    public:
        virtual ~Node() {}

        /// Intersects the node with a ray, returns either a `Hit` or nothing.
        /// If an intersection is found, the `tmax` parameter of the ray is updated.
        virtual std::optional<Hit> intersect(proto::Rayf&) const = 0;
        /// Tests if a given ray is occluded or not.
        virtual bool is_occluded(const proto::Rayf&) const = 0;
    };

    std::unique_ptr<Node> root;

    unique_vector<Bsdf>     bsdfs;
    unique_vector<Light>    lights;
    unique_vector<Texture>  textures;
    unique_vector<Material> materials;
};

} // namespace sol

#endif
