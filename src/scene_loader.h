#ifndef SOL_SCENE_LOADER_H
#define SOL_SCENE_LOADER_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <toml++/toml.h>

#include "sol/scene.h"

namespace sol {

/// Internal object used by scene file loaders.
class SceneLoader {
public:
    SceneLoader(Scene&, const Scene::Defaults&, std::ostream*);
    ~SceneLoader();

    // Loads the given file into the scene.
    bool load(const std::string_view& file_name);

    // Loads the given file into the scene using the given directory as
    // a base directory when loading files with relative paths.
    bool load(const std::string_view& file_name, const std::string_view& base_dir);

private:
    static proto::Vec3f parse_vec3(toml::node_view<const toml::node>, const proto::Vec3f&);
    std::unique_ptr<Camera> parse_camera(const toml::table&);
    void parse_node(const toml::table&);

    Scene& scene_;
    const Scene::Defaults& defaults_;
    std::ostream* err_out_;

    std::unordered_map<std::string, const Bsdf*>    bsdfs_;
    std::unordered_map<std::string, const Light*>   lights_;
    std::unordered_map<std::string, const Texture*> textures_;
    std::unordered_map<std::string, const Image*>   images_;

    std::unordered_map<std::string, std::unique_ptr<Scene::Node>> nodes_;
};

} // namespace sol

#endif
