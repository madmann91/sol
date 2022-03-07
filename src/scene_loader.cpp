#include <filesystem>
#include <system_error>

#include "scene_loader.h"
#include "formats/obj.h"

#include "sol/cameras.h"
#include "sol/bsdfs.h"
#include "sol/lights.h"
#include "sol/image.h"
#include "sol/geometry.h"

namespace sol {

static proto::Vec3f parse_vec3(toml::node_view<const toml::node> node, const proto::Vec3f& default_val) {
    if (auto array = node.as_array(); array) {
        return proto::Vec3f(
            (*array)[0].value_or(default_val[0]),
            (*array)[1].value_or(default_val[1]),
            (*array)[2].value_or(default_val[2]));
    }
    return default_val;
}

SceneLoader::SceneLoader(Scene& scene, const Scene::Defaults& defaults, std::ostream* err_out)
    : scene_(scene), defaults_(defaults), err_out_(err_out)
{}

SceneLoader::~SceneLoader() = default;

bool SceneLoader::load(const std::string& file_name) {
    try {
        load_and_throw_on_error(file_name);
        return true;
    } catch (toml::parse_error& error) {
        // Use the same error message syntax for TOML++ errors and the loaders' error messages
        if (err_out_) (*err_out_) << SourceError::from_toml(error).what();
    } catch (std::exception& e) {
        if (err_out_) (*err_out_) << e.what();
    }
    return false;
}

void SceneLoader::load_and_throw_on_error(const std::string& file_name) {
    std::ifstream is(file_name);
    if (!is)
        throw std::runtime_error("Cannot open scene file '" + file_name + "'");

    std::error_code err_code;
    auto base_dir = std::filesystem::absolute(file_name, err_code).parent_path().string();

    auto table = toml::parse(is, file_name);
    if (auto camera = table["camera"].as_table())
        scene_.camera = create_camera(*camera);
    if (auto geoms = table["geoms"].as_array()) {
        for (auto& geom : *geoms) {
            if (auto table = geom.as_table())
                create_geom(*table, base_dir);
        }
    }
    auto root_name = table["root"].value_or<std::string>("");
    if (!geoms_.contains(root_name))
        throw std::runtime_error("Root geometry named '" + root_name + "' cannot be found");
    scene_.root = std::move(geoms_[root_name]);
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

void SceneLoader::insert_geom(const std::string& name, std::unique_ptr<Geometry>&& geom) {
    if (!geoms_.emplace(name, std::move(geom)).second)
        throw std::runtime_error("Duplicate geometry found with name '" + name + "'");
}

std::unique_ptr<Camera> SceneLoader::create_camera(const toml::table& table) {
    auto type = table["type"].value_or<std::string>("");
    if (type == "perspective") {
        auto eye = parse_vec3(table["eye"], defaults_.eye_pos);
        auto dir = parse_vec3(table["dir"], defaults_.dir_vector);
        auto up  = parse_vec3(table["up"], defaults_.up_vector);
        auto fov = table["fov"].value_or(defaults_.fov);
        auto ratio = table["aspect"].value_or(defaults_.aspect_ratio);
        return std::make_unique<PerspectiveCamera>(eye, dir, up, fov, ratio);
    }
    throw SourceError::from_toml(table.source(), "Unknown camera type '" + type + "'");
}

void SceneLoader::create_geom(const toml::table& table, const std::string& base_dir) {
    auto name = table["name"].value_or<std::string>("");
    auto type = table["type"].value_or<std::string>("");
    if (type == "import") {
        auto file = table["file"].value_or<std::string>("");
        if (file.ends_with(".obj"))
            return insert_geom(name, obj::load(*this, base_dir + "/" + file));
        throw SourceError::from_toml(table.source(), "Unknown file format for '" + file + "'");
    }
    throw SourceError::from_toml(table.source(), "Unknown node type '" + type + "'");
}

std::optional<Scene> Scene::load(const std::string& file_name, const Defaults& defaults, std::ostream* err_out) {
    Scene scene;
    SceneLoader loader(scene, defaults, err_out);
    return loader.load(file_name) ? std::make_optional(std::move(scene)) : std::nullopt;
}

} // namespace sol
