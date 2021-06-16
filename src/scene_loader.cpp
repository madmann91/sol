#include <filesystem>
#include <system_error>
#include <unordered_map>

#include <toml++/toml.h>

#include "sol/scene.h"
#include "sol/cameras.h"
#include "sol/bsdfs.h"
#include "sol/lights.h"

namespace sol {

class SceneLoader {
public:
    SceneLoader(Scene& scene, const Scene::Defaults& defaults, std::ostream* err_out)
        : scene_(scene), defaults_(defaults), err_out_(err_out)
    {}

    // Loads the given file into the scene.
    bool load(const std::string_view& file_name);

    // Loads the given file into the scene using the given directory as
    // a base directory when loading files with relative paths.
    bool load(const std::string_view& file_name, const std::string_view& base_dir);

private:
    proto::Vec3f parse_vec3(toml::node_view<const toml::node>, const proto::Vec3f& = proto::Vec3f(0, 0, 0));
    std::unique_ptr<Camera> parse_camera(const toml::table&);

    Scene& scene_;
    const Scene::Defaults& defaults_;
    std::ostream* err_out_;
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
        if (auto camera_table = table["camera"].as_table())
            scene_.camera = parse_camera(*camera_table);
        //create_nodes(table["nodes"].as_array());
    } catch (std::exception& e) {
        if (err_out_) (*err_out_) << e.what();
        return false;
    }
    return true;
}

proto::Vec3f SceneLoader::parse_vec3(toml::node_view<const toml::node> node, const proto::Vec3f& default_val) {
    if (auto array = node.as_array(); array && array->size() == 3) {
        return proto::Vec3f(
            (*array)[0].value_or(default_val[0]),
            (*array)[1].value_or(default_val[1]),
            (*array)[2].value_or(default_val[2]));
    }
    return default_val;
}

std::unique_ptr<Camera> SceneLoader::parse_camera(const toml::table& table) {
    auto type = table["type"].value_or<std::string>("");
    if (type == "perspective") {
        auto eye = parse_vec3(table["eye"], defaults_.eye_pos);
        auto dir = parse_vec3(table["dir"], defaults_.dir_vector);
        auto up  = parse_vec3(table["up"], defaults_.up_vector);
        auto fov = table["fov"].value_or(defaults_.fov);
        auto ratio = table["aspect"].value_or(defaults_.aspect_ratio);
        return std::make_unique<PerspectiveCamera>(eye, dir, up, fov, ratio);
    }
    throw std::runtime_error("unknown camera type '" + type + "'");
}

bool Scene::load(const std::string_view& file_name, const Defaults& defaults, std::ostream* err_out) {
    SceneLoader loader(*this, defaults, err_out);
    return loader.load(file_name);
}

} // namespace sol
