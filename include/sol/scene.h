#ifndef SOL_SCENE_H
#define SOL_SCENE_H

#include <memory>
#include <vector>
#include <optional>
#include <ostream>
#include <string>

#include <proto/vec.h>

namespace sol {

class Bsdf;
class Light;
class Texture;
class Camera;
class Image;
class Geometry;

namespace detail {

struct SceneDefaults {
    float fov = 60.0f;
    float aspect_ratio = 1.0f;
    proto::Vec3f eye_pos    = proto::Vec3f(0, 0, 0);
    proto::Vec3f dir_vector = proto::Vec3f(0, 0, 1);
    proto::Vec3f up_vector  = proto::Vec3f(0, 1, 0);
};

} // namespace detail

/// Owning collection of lights, BSDFs, textures and geometric objects that make up a scene.
struct Scene {
    Scene();

    Scene(Scene&&);
    ~Scene();

    std::unique_ptr<Geometry> root;
    std::unique_ptr<Camera>   camera;

    template <typename T> using unique_vector = std::vector<std::unique_ptr<T>>;

    unique_vector<Bsdf>    bsdfs;
    unique_vector<Light>   lights;
    unique_vector<Texture> textures;
    unique_vector<Image>   images;

    using Defaults = detail::SceneDefaults;

    /// Loads the given scene file, using the given configuration to deduce missing values.
    static std::optional<Scene> load(
        const std::string& file_name,
        const Defaults& defaults = {},
        std::ostream* err_out = nullptr);
};

} // namespace sol

#endif
