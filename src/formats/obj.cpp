#include <vector>
#include <string>
#include <optional>
#include <istream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <exception>
#include <cstring>
#include <filesystem>
#include <system_error>

#include <proto/hash.h>

#include "sol/color.h"
#include "sol/textures.h"
#include "sol/bsdfs.h"
#include "sol/lights.h"
#include "sol/triangle_mesh.h"

#include "formats/obj.h"

namespace sol::obj {

// Maximum line length allowed in OBJ and MTL files.
static constexpr size_t max_line_len = 1024;

struct Index {
    int v, n, t;

    size_t hash() const {
        return proto::fnv::Hasher().combine(v).combine(n).combine(t);
    }

    bool operator == (const Index& other) const {
        return v == other.v && n == other.n && t == other.t;
    }
};

struct Face {
    std::vector<Index> indices;
    size_t material;
};

struct Group {
    std::vector<Face> faces;
};

struct Object {
    std::vector<Group> groups;
};

struct Material {
    RgbColor ka = RgbColor::black();
    RgbColor kd = RgbColor::black();
    RgbColor ks = RgbColor::black();
    RgbColor ke = RgbColor::black();
    RgbColor tf = RgbColor::black();
    float ns = 0.0f;
    float ni = 0.0f;
    float tr = 0.0f;
    float d = 0.0f;
    int illum = 0;
    std::string map_ka;
    std::string map_kd;
    std::string map_ks;
    std::string map_ns;
    std::string map_ke;
    std::string map_d;
    std::string bump;
};

using MaterialLib = std::unordered_map<std::string, Material>;

struct File {
    std::vector<Object> objects;
    std::vector<proto::Vec3f> vertices;
    std::vector<proto::Vec3f> normals;
    std::vector<proto::Vec2f> tex_coords;
    std::vector<std::string> materials;
    std::unordered_set<std::string> mtl_files;

    size_t face_count() const {
        size_t count = 0;
        for (auto& object : objects) {
            for (auto& group : object.groups)
                count += group.faces.size();
        }
        return count;
    }
};

inline void remove_eol(char* ptr) {
    int i = 0;
    while (ptr[i]) i++;
    i--;
    while (i > 0 && std::isspace(ptr[i])) {
        ptr[i] = '\0';
        i--;
    }
}

inline char* strip_text(char* ptr) {
    while (*ptr && !std::isspace(*ptr)) { ptr++; }
    return ptr;
}

inline char* strip_spaces(char* ptr) {
    while (std::isspace(*ptr)) { ptr++; }
    return ptr;
}

inline std::optional<Index> parse_index(char** ptr) {
    char* base = *ptr;

    // Detect end of line (negative indices are supported) 
    base = strip_spaces(base);
    if (!std::isdigit(*base) && *base != '-')
        return std::nullopt;

    Index idx = {0, 0, 0};
    idx.v = std::strtol(base, &base, 10);

    base = strip_spaces(base);
    if (*base == '/') {
        base++;

        if (*base != '/')
            idx.t = std::strtol(base, &base, 10);

        base = strip_spaces(base);
        if (*base == '/') {
            base++;
            idx.n = std::strtol(base, &base, 10);
        }
    }

    *ptr = base;
    return std::make_optional(idx);
}

inline proto::Vec3f parse_vec3(char* ptr) {
    proto::Vec3f v;
    v[0] = std::strtof(ptr, &ptr);
    v[1] = std::strtof(ptr, &ptr);
    v[2] = std::strtof(ptr, &ptr);
    return v;
}

inline RgbColor parse_rgb_color(char* ptr) {
    RgbColor color;
    color.r = std::max(std::strtof(ptr, &ptr), 0.0f);
    color.g = std::max(std::strtof(ptr, &ptr), 0.0f);
    color.b = std::max(std::strtof(ptr, &ptr), 0.0f);
    return color;
}

inline proto::Vec2f parse_vec2(char* ptr) {
    proto::Vec2f v;
    v[0] = std::strtof(ptr, &ptr);
    v[1] = std::strtof(ptr, &ptr);
    return v;
}

static File parse_obj(std::istream& is, const std::string& file_name, bool is_strict) {
    File file;

    // Add dummy elements to account for the fact that indices start at 1 in the file.
    file.objects.emplace_back();
    file.objects[0].groups.emplace_back();
    file.materials.emplace_back("dummy");
    file.vertices.emplace_back(0);
    file.normals.emplace_back(0);
    file.tex_coords.emplace_back(0);

    char line[max_line_len];
    size_t line_count = 0;
    size_t material_index = 0;

    while (is.getline(line, max_line_len)) {
        line_count++;
        char* ptr = strip_spaces(line);

        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;

        remove_eol(ptr);

        // Test each command in turn, the most frequent first
        if (*ptr == 'v') {
            switch (ptr[1]) {
                case ' ':
                case '\t': file.vertices  .push_back(parse_vec3(ptr + 1)); break;
                case 'n':  file.normals   .push_back(parse_vec3(ptr + 2)); break;
                case 't':  file.tex_coords.push_back(parse_vec2(ptr + 2)); break;
                default:
                    if (is_strict)
                        throw SourceError(file_name, { line_count, 1 }, "Invalid vertex");
                    break;
            }
        } else if (*ptr == 'f' && std::isspace(ptr[1])) {
            Face face;
            ptr += 2;
            while (true) {
                auto index = parse_index(&ptr);
                if (!index)
                    break;
                face.indices.push_back(*index);
            }
            face.material = material_index;

            // Convert relative indices to absolute
            bool is_valid = face.indices.size() >= 3;
            for (size_t i = 0; i < face.indices.size(); i++) {
                face.indices[i].v = (face.indices[i].v < 0) ? file.vertices.size()   + face.indices[i].v : face.indices[i].v;
                face.indices[i].t = (face.indices[i].t < 0) ? file.tex_coords.size() + face.indices[i].t : face.indices[i].t;
                face.indices[i].n = (face.indices[i].n < 0) ? file.normals.size()    + face.indices[i].n : face.indices[i].n;
                is_valid &= face.indices[i].v > 0 && face.indices[i].t >= 0 && face.indices[i].n >= 0;
            }

            if (is_valid)
                file.objects.back().groups.back().faces.push_back(face);
            else if (is_strict)
                throw SourceError(file_name, { line_count, 1 }, "Invalid face");
        } else if (*ptr == 'g' && std::isspace(ptr[1])) {
            file.objects.back().groups.emplace_back();
        } else if (*ptr == 'o' && std::isspace(ptr[1])) {
            file.objects.emplace_back();
            file.objects.back().groups.emplace_back();
        } else if (!std::strncmp(ptr, "usemtl", 6) && std::isspace(ptr[6])) {
            ptr = strip_spaces(ptr + 6);
            char* base = ptr;
            ptr = strip_text(ptr);

            std::string material(base, ptr);
            if (auto it = std::find(file.materials.begin(), file.materials.end(), material); it != file.materials.end()) {
                material_index = it - file.materials.begin();
            } else {
                material_index = file.materials.size();
                file.materials.push_back(material);
            }
        } else if (!std::strncmp(ptr, "mtllib", 6) && std::isspace(ptr[6])) {
            ptr = strip_spaces(ptr + 6);
            char* base = ptr;
            ptr = strip_text(ptr);
            file.mtl_files.emplace(base, ptr);
        } else if (*ptr == 's' && std::isspace(ptr[1])) {
            // Ignore smooth commands
        } else if (is_strict)
            throw SourceError(file_name, { line_count, 1 }, "Unknown command");
    }

    return file;
}

static File parse_obj(const std::string& file_name, bool is_strict) {
    std::ifstream is(file_name);
    if (!is)
        throw std::runtime_error("Cannot open OBJ file '" + file_name + "'");
    return parse_obj(is, file_name, is_strict);
}

static void parse_mtl(std::istream& stream, const std::string& file_name, MaterialLib& material_lib, bool is_strict = false) {
    char line[max_line_len];
    size_t line_count = 0;

    auto* material = &material_lib["dummy"];
    while (stream.getline(line, max_line_len)) {
        line_count++;
        char* ptr = strip_spaces(line);

        // Skip comments and empty lines
        if (*ptr == '\0' || *ptr == '#')
            continue;

        remove_eol(ptr);
        if (!std::strncmp(ptr, "newmtl", 6) && std::isspace(ptr[6])) {
            ptr = strip_spaces(ptr + 7);
            char* base = ptr;
            ptr = strip_text(ptr);
            std::string name(base, ptr);
            if (is_strict && material_lib.contains(name))
                throw SourceError(file_name, { line_count, 1 }, "Redefinition of material '" + name + "'");
            material = &material_lib[name];
        } else if (ptr[0] == 'K' && std::isspace(ptr[2])) {
            if (ptr[1] == 'a')
                material->ka = parse_rgb_color(ptr + 3);
            else if (ptr[1] == 'd')
                material->kd = parse_rgb_color(ptr + 3);
            else if (ptr[1] == 's')
                material->ks = parse_rgb_color(ptr + 3);
            else if (ptr[1] == 'e')
                material->ke = parse_rgb_color(ptr + 3);
            else
                goto unknown_command;
        } else if (ptr[0] == 'N' && std::isspace(ptr[2])) {
            if (ptr[1] == 's')
                material->ns = std::strtof(ptr + 3, &ptr);
            else if (ptr[1] == 'i')
                material->ni = std::strtof(ptr + 3, &ptr);
            else
                goto unknown_command;
        } else if (ptr[0] == 'T' && std::isspace(ptr[2])) {
            if (ptr[1] == 'f')
                material->tf = parse_rgb_color(ptr + 3);
            else if (ptr[1] == 'r')
                material->tr = std::strtof(ptr + 3, &ptr);
            else
                goto unknown_command;
        } else if (ptr[0] == 'd' && std::isspace(ptr[1])) {
            material->d = std::strtof(ptr + 2, &ptr);
        } else if (!std::strncmp(ptr, "illum", 5) && std::isspace(ptr[5])) {
            material->illum = std::strtof(ptr + 6, &ptr);
        } else if (!std::strncmp(ptr, "map_Ka", 6) && std::isspace(ptr[6])) {
            material->map_ka = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Kd", 6) && std::isspace(ptr[6])) {
            material->map_kd = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Ks", 6) && std::isspace(ptr[6])) {
            material->map_ks = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Ke", 6) && std::isspace(ptr[6])) {
            material->map_ke = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_Ns", 6) && std::isspace(ptr[6])) {
            material->map_ns = std::string(strip_spaces(ptr + 7));
        } else if (!std::strncmp(ptr, "map_d", 5) && std::isspace(ptr[5])) {
            material->map_d = std::string(strip_spaces(ptr + 6));
        } else if (!std::strncmp(ptr, "bump", 4) && std::isspace(ptr[4])) {
            material->bump = std::string(strip_spaces(ptr + 4));
        } else
            goto unknown_command;
        continue;

unknown_command:
        if (is_strict)
            throw SourceError(file_name, { line_count, 1 }, "Unknown command '" + std::string(ptr) + "'");
    }
}

static void parse_mtl(const std::string& file_name, MaterialLib& material_lib, bool is_strict) {
    std::ifstream is(file_name);
    if (!is) {
        // Accept missing material files in non-strict mode
        if (!is_strict) return;
        throw std::runtime_error("Cannot open MTL file '" + file_name + "'");
    }
    parse_mtl(is, file_name, material_lib, is_strict);
}

template <typename T>
static const Texture* get_texture(
    SceneLoader& scene_loader,
    const std::string& file_name,
    const T& constant,
    bool is_strict)
{
    using ImageTextureType = ImageTexture<ImageFilter::Bilinear, BorderMode::Repeat>;
    if (file_name != "") {
        if (auto image = scene_loader.load_image(file_name))
            return scene_loader.get_or_insert_texture<ImageTextureType>(*image);
        else if (is_strict)
            throw std::runtime_error("Cannot load image '" + file_name + "'");
    }
    return scene_loader.get_or_insert_texture<ConstantTextureType<T>>(constant);
}

static const ColorTexture* get_color_texture(
    SceneLoader& scene_loader,
    const std::string& file_name,
    const RgbColor& color,
    bool is_strict)
{
    return static_cast<const ColorTexture*>(get_texture(scene_loader, file_name, color, is_strict));
}

static const Bsdf* convert_material(SceneLoader& scene_loader, const Material& material, bool is_strict) {
    switch (material.illum) {
        case 5: {
            auto ks = get_color_texture(scene_loader, material.map_ks, material.ks, is_strict);
            return scene_loader.get_or_insert_bsdf<MirrorBsdf>(*ks);
        }
        case 7: {
            auto ks = get_color_texture(scene_loader, material.map_ks, material.ks, is_strict);
            auto kt = scene_loader.get_or_insert_texture<ConstantColorTexture>(material.tf);
            auto ni = scene_loader.get_or_insert_texture<ConstantTexture>(1.0f / material.ni);
            return scene_loader.get_or_insert_bsdf<GlassBsdf>(*ks, static_cast<const ColorTexture&>(*kt), *ni);
        }
        default: {
            // A mix of Phong and diffuse
            const Bsdf* diff = nullptr, *spec = nullptr;
            float diff_k = 0, spec_k = 0;

            if (material.ks != RgbColor::black() || material.map_ks != "") {
                auto ks = get_color_texture(scene_loader, material.map_ks, material.ks, is_strict);
                auto ns = get_texture(scene_loader, material.map_ns, material.ns, is_strict);
                spec = scene_loader.get_or_insert_bsdf<PhongBsdf>(*ks, *ns);
                spec_k = material.map_ks != "" ? 1.0f : material.ks.max_component();
            }

            if (material.kd != RgbColor::black() || material.map_kd != "") {
                auto kd = get_color_texture(scene_loader, material.map_kd, material.kd, is_strict);
                diff = scene_loader.get_or_insert_bsdf<DiffuseBsdf>(*kd);
                diff_k = material.map_kd != "" ? 1.0f : material.kd.max_component();
            }

            if (spec && diff) {
                auto k = scene_loader.get_or_insert_texture<ConstantTexture>(spec_k / (diff_k + spec_k));
                return scene_loader.get_or_insert_bsdf<InterpBsdf>(diff, spec, *k);
            }
            return diff ? diff : spec;
        }
    }
}

static void check_materials(File& file, const MaterialLib& material_lib, bool is_strict) {
    for (auto& material : file.materials) {
        if (!material_lib.contains(material)) {
            if (is_strict)
                throw std::runtime_error("Cannot find material named '" + material + "'");
            material = "dummy";
        }
    }
}

static std::unique_ptr<Scene::Node> build_mesh(
    SceneLoader& scene_loader,
    const File& file,
    const MaterialLib& material_lib,
    bool is_strict)
{
    auto hash = [] (const Index& idx) { return idx.hash(); };
    std::unordered_map<Index, size_t, decltype(hash)> index_map(file.vertices.size(), std::move(hash));

    std::vector<size_t> indices;
    std::vector<proto::Vec3f> vertices;
    std::vector<proto::Vec3f> normals;
    std::vector<proto::Vec2f> tex_coords;
    std::vector<const Bsdf*> bsdfs;
    std::vector<size_t> normals_to_fix;
    std::unordered_map<size_t, const TriangleLight*> lights;

    vertices.reserve(file.vertices.size());
    normals.reserve(file.normals.size());
    tex_coords.reserve(file.tex_coords.size());
    indices.reserve(file.face_count() * 3);

    for (auto& object : file.objects) {
        for (auto& group : object.groups) {
            for (auto& face : group.faces) {
                // Make a unique vertex for each possible combination of
                // vertex, texture coordinate, and normal index.
                for (auto& index : face.indices) {
                    if (index_map.emplace(index, index_map.size()).second) {
                        // Mark this normal so that we can fix it later,
                        // if it is missing from the OBJ file.
                        if (index.n == 0)
                            normals_to_fix.push_back(normals.size());
                        vertices  .push_back(file.vertices  [index.v]);
                        normals   .push_back(file.normals   [index.n]);
                        tex_coords.push_back(file.tex_coords[index.t]);
                    }
                }

                // Get a BSDF for the face
                auto& material = material_lib.at(file.materials[face.material]);
                auto bsdf = convert_material(scene_loader, material, is_strict);
                bool is_emissive = material.ke != Color::black() || material.map_ke != "";

                // Add triangles to the mesh
                size_t first_index = index_map[face.indices[0]];
                size_t cur_index = index_map[face.indices[1]];
                for (size_t i = 1, n = face.indices.size() - 1; i < n; ++i) {
                    auto next_index = index_map[face.indices[i + 1]];

                    if (is_emissive) {
                        proto::Trianglef triangle(vertices[first_index], vertices[cur_index], vertices[next_index]);
                        auto intensity = get_color_texture(scene_loader, material.map_ke, material.ke, is_strict);
                        auto light = scene_loader.get_or_insert_light<TriangleLight>(triangle, *intensity);
                        lights.emplace(indices.size() / 3, static_cast<const TriangleLight*>(light));
                    }

                    indices.push_back(first_index);
                    indices.push_back(cur_index);
                    indices.push_back(next_index);
                    bsdfs.push_back(bsdf);

                    cur_index = next_index;
                }
            }
        }
    }

    // Fix normals that are missing.
    if (!normals_to_fix.empty()) {
        std::vector<proto::Vec3f> smooth_normals(normals.size(), proto::Vec3f(0));
        for (size_t i = 0; i < indices.size(); i += 3) {
            auto normal = proto::Trianglef(
                vertices[indices[i + 0]],
                vertices[indices[i + 1]],
                vertices[indices[i + 2]]).normal();
            smooth_normals[indices[i + 0]] += normal;
            smooth_normals[indices[i + 1]] += normal;
            smooth_normals[indices[i + 2]] += normal;
        }
        for (auto normal_index : normals_to_fix)
            normals[normal_index] = proto::normalize(smooth_normals[normal_index]);
    }

    return std::make_unique<TriangleMesh>(
        std::move(indices),
        std::move(vertices),
        std::move(normals),
        std::move(tex_coords),
        std::move(bsdfs),
        std::move(lights));
}

std::unique_ptr<Scene::Node> load(SceneLoader& scene_loader, const std::string_view& file_name) {
    static constexpr bool is_strict = false;

    auto file = parse_obj(std::string(file_name), is_strict);
    MaterialLib material_lib;
    for (auto& mtl_file : file.mtl_files) {
        std::error_code err_code;
        auto full_path = std::filesystem::absolute(file_name, err_code).parent_path().string() + "/" + mtl_file;
        parse_mtl(full_path, material_lib, is_strict);
    }

    check_materials(file, material_lib, is_strict);
    return build_mesh(scene_loader, file, material_lib, is_strict);
}

} // namespace sol::obj
