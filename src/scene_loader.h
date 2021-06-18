#ifndef SOL_SCENE_LOADER_H
#define SOL_SCENE_LOADER_H

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include <toml++/toml.h>

#include <proto/hash.h>

#include "sol/scene.h"

namespace sol {

/// Exception used internally to represent an error in a source file
struct SourceError : public std::runtime_error {
    struct Pos { size_t row, col; };

    SourceError(const std::string_view& file_name, const Pos& pos, const std::string_view& message)
        : std::runtime_error(format(file_name, pos, message))
    {}

    static std::string format(const std::string_view& file_name, const Pos& pos, const std::string_view& message) {
        return
            std::string(message) + " (in " +
            std::string(file_name) + ":" +
            std::to_string(pos.row) + ":" +
            std::to_string(pos.col) + ")";
    }
};

/// Internal object used by scene file loaders.
/// This object performs hash-consing of scene objects,
/// so that the same objects are re-used whenever possible.
struct SceneLoader {
public:
    SceneLoader(Scene&, const Scene::Defaults&, std::ostream*);
    ~SceneLoader();

    // Loads the given file into the scene.
    bool load(const std::string& file_name);

    // Loads an image or returns an already loaded one.
    const Image* load_image(const std::string& file_name);

    // Creates a new BSDF or returns an existing one.
    template <typename T, typename... Args>
    const Bsdf* get_or_insert_bsdf(Args&&... args) {
        return get_or_insert<T, Bsdf>(bsdfs_, scene_.bsdfs, std::forward<Args>(args)...); 
    }

    // Creates a new texture or returns an existing one.
    template <typename T, typename... Args>
    const Texture* get_or_insert_texture(Args&&... args) {
        return get_or_insert<T, Texture>(textures_, scene_.textures, std::forward<Args>(args)...); 
    }

    // Creates a new light or returns an existing one.
    template <typename T, typename... Args>
    const Light* get_or_insert_light(Args&&... args) {
        return get_or_insert<T, Light>(lights_, scene_.lights, std::forward<Args>(args)...); 
    }

private:
    static proto::Vec3f parse_vec3(toml::node_view<const toml::node>, const proto::Vec3f&);
    std::unique_ptr<Camera> parse_camera(const toml::table&);
    void parse_node(const toml::table&, const std::string&);
    void parse_scene(const toml::table&);

    template <typename T, typename U, typename Set, typename Container, typename... Args>
    static const U* get_or_insert(Set& set, Container& container, Args&&... args) {
        T t(std::forward<Args>(args)...);
        if (auto it = set.find(&t); it != set.end())
            return *it;
        auto new_t = new T(std::move(t));
        container.emplace_back(new_t);
        set.insert(new_t);
        return new_t;
    }

    struct Hash {
        template <typename T>
        size_t operator () (const T* t) const {
            proto::fnv::Hasher hasher;
            return t->hash(hasher);
        }
    };

    struct Compare {
        template <typename T>
        bool operator () (const T* left, const T* right) const {
            return left->equals(*right);
        }
    };

    Scene& scene_;
    const Scene::Defaults& defaults_;

    std::unordered_set<const Bsdf*, Hash, Compare>    bsdfs_;
    std::unordered_set<const Light*, Hash, Compare>   lights_;
    std::unordered_set<const Texture*, Hash, Compare> textures_;

    std::unordered_map<std::string, const Image*> images_;
    std::unordered_map<std::string, std::unique_ptr<Scene::Node>> nodes_;

    std::ostream* err_out_;
};

} // namespace sol

#endif
