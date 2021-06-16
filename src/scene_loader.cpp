#include <filesystem>
#include <system_error>

#include <toml++/toml.h>

#include "sol/scene.h"

namespace sol {

class SceneLoader {
public:
    SceneLoader(Scene& scene)
        : scene_(scene)
    {}

    // Loads the given file into the scene.
    bool load(const std::string_view& file_name);

    // Loads the given file into the scene using the given directory as
    // a base directory when loading files with relative paths.
    bool load(const std::string_view& file_name, const std::string_view& base_dir);

private:
    Scene& scene_;
    std::unordered_map<std::string, const Bsdf*>    bsdfs_;
    std::unordered_map<std::string, const Light*>   lights_;
    std::unordered_map<std::string, const Texture*> textures_;
    std::unordered_map<std::string, const Image*>   images_;
};

bool SceneLoader::load(const std::string_view& file_name) {
    std::error_code err_code;
    auto base_dir = std::filesystem::absolute(file_name, err_code).parent_path();
    return !err_code ? load(file_name, base_dir.string()) : false;
}

bool SceneLoader::load(const std::string_view& file_name, const std::string_view& base_dir) {
    try {
        auto table = toml::parse_file(file_name);
        //setup_camera(table["camera"].as_table());
        //create_nodes(table["nodes"].as_array());
    } catch (std::exception& e) {
        return false;
    }
    return true;
}

bool Scene::load(const std::string_view& file_name) {
    SceneLoader loader(*this);
    return loader.load(file_name);
}

} // namespace sol
