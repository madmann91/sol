#include <filesystem>
#include <system_error>

#include "scene_loader.h"
#include "formats/obj.h"

#include "sol/cameras.h"
#include "sol/bsdfs.h"
#include "sol/lights.h"
#include "sol/image.h"

namespace sol {

SceneLoader::SceneLoader(Scene& scene, const Scene::Defaults& defaults, std::ostream* err_out)
    : scene_(scene), defaults_(defaults), err_out_(err_out)
{}

SceneLoader::~SceneLoader() = default;

bool SceneLoader::load(const std::string_view& file_name) {
    try {
        auto table = toml::parse_file(file_name);
        if (auto camera = table["camera"].as_table())
            scene_.camera = parse_camera(*camera);
        if (auto nodes = table["nodes"].as_array()) {
            for (auto& node : *nodes) {
                if (auto table = node.as_table())
                    parse_node(*table);
            }
        }
    } catch (std::exception& e) {
        if (err_out_) (*err_out_) << e.what();
        return false;
    }
    return true;
}

const Image* SceneLoader::load_image(const std::string& file_name) {
    std::error_code err_code;
    auto full_name = std::filesystem::absolute(file_name, err_code).string();
    if (auto it = images_.find(full_name); it != images_.end())
        return it->second;
    if (auto image = Image::load(full_name)) {
        auto image_ptr = scene_.images.emplace_back(new Image(std::move(*image))).get();
        images_.emplace(full_name, image_ptr);
        return image_ptr;
    }
    return nullptr;
}

static inline SourceError error_at(const toml::node& node, const std::string_view& message) {
    return SourceError(message, { node.source().begin.line, node.source().begin.column }, *node.source().path);
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
    throw error_at(table, "unknown camera type '" + type + "'");
}

void SceneLoader::parse_node(const toml::table& table) {
    auto name = table["name"].value_or<std::string>("");
    auto type = table["type"].value_or<std::string>("");
    if (type == "import") {
        auto file = table["file"].value_or<std::string>("");
        if (file.ends_with(".obj"))
            nodes_["name"] = obj::load(*this, file);
        else
            throw error_at(table, "unknown file format for '" + file + "'");
    }
    throw error_at(table, "unknown node type '" + type + "'");
}

bool Scene::load(const std::string_view& file_name, const Defaults& defaults, std::ostream* err_out) {
    SceneLoader loader(*this, defaults, err_out);
    return loader.load(file_name);
}

} // namespace sol
