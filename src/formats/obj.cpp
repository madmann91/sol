#include <vector>
#include <string>
#include <optional>
#include <istream>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <exception>
#include <cstring>

#include "sol/color.h"

#include "formats/obj.h"

namespace sol::obj {

// Maximum line length allowed in OBJ and MTL files.
static constexpr size_t max_line_len = 1024;

struct Index {
    int v, n, t;
};

struct Face {
    std::vector<Index> indices;
    int material;
};

struct Group {
    std::vector<Face> faces;
};

struct Object {
    std::vector<Group> groups;
};

struct Material {
    RgbColor ka;
    RgbColor kd;
    RgbColor ks;
    RgbColor ke;
    float ns;
    float ni;
    RgbColor tf;
    float tr;
    float d;
    int illum;
    std::string map_ka;
    std::string map_kd;
    std::string map_ks;
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

inline RgbColor parse_rgb(char* ptr) {
    RgbColor color;
    color.r = std::strtof(ptr, &ptr);
    color.g = std::strtof(ptr, &ptr);
    color.b = std::strtof(ptr, &ptr);
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
    file.vertices.emplace_back();
    file.normals.emplace_back();
    file.tex_coords.emplace_back();

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
                        throw SourceError(file_name, { line_count, 1 }, "invalid vertex");
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
                is_valid &= face.indices[i].v > 0 && face.indices[i].t > 0 && face.indices[i].n > 0;
            }

            if (is_valid)
                file.objects.back().groups.back().faces.push_back(face);
            else if (is_strict)
                throw SourceError(file_name, { line_count, 1 }, "invalid face");
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
                file.materials.push_back(material);
                material_index = file.materials.size();
            }
        } else if (!std::strncmp(ptr, "mtllib", 6) && std::isspace(ptr[6])) {
            ptr = strip_spaces(ptr + 6);
            char* base = ptr;
            ptr = strip_text(ptr);
            file.mtl_files.emplace(base, ptr);
        } else if (*ptr == 's' && std::isspace(ptr[1])) {
            // Ignore smooth commands
        } else if (is_strict)
            throw SourceError(file_name, { line_count, 1 }, "unknown command");
    }

    return file;
}

static File parse_obj(const std::string& file_name, bool is_strict) {
    std::ifstream is(file_name);
    if (!is)
        throw std::runtime_error("cannot open OBJ file '" + file_name + "'");
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
            material = &material_lib[std::string(base, ptr)];
        } else if (ptr[0] == 'K' && std::isspace(ptr[2])) {
            if (ptr[1] == 'a')
                material->ka = parse_rgb(ptr + 3);
            else if (ptr[1] == 'd')
                material->kd = parse_rgb(ptr + 3);
            else if (ptr[1] == 's')
                material->ks = parse_rgb(ptr + 3);
            else if (ptr[1] == 'e')
                material->ke = parse_rgb(ptr + 3);
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
                material->tf = parse_rgb(ptr + 3);
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
        } else if (!std::strncmp(ptr, "map_d", 5) && std::isspace(ptr[5])) {
            material->map_d = std::string(strip_spaces(ptr + 6));
        } else if (!std::strncmp(ptr, "bump", 4) && std::isspace(ptr[4])) {
            material->bump = std::string(strip_spaces(ptr + 4));
        } else
            goto unknown_command;
        continue;

unknown_command:
        if (is_strict)
            throw SourceError(file_name, { line_count, 1 }, "unknown command '" + std::string(ptr) + "'");
    }
}

static void parse_mtl(const std::string& file_name, MaterialLib& material_lib, bool is_strict) {
    std::ifstream is(file_name);
    if (!is) {
        // Accept missing material files in non-strict mode
        if (!is_strict) return;
        throw std::runtime_error("cannot open MTL file '" + file_name + "'");
    }
    parse_mtl(is, file_name, material_lib, is_strict);
}

static void convert_materials(SceneLoader& scene_loader, const MaterialLib& material_lib, bool is_strict) {
    for (const auto& [name, material] : material_lib) {
        /*const Bsdf* bsdf = nullptr;
        switch (material.illum) {
            case 5: bsdf = new MirrorBsdf(mat.ks); break;
            case 7: bsdf = new GlassBsdf(1.0f, mat.ni, mat.ks, mat.tf); break;
            default:
                Texture* diff_tex = nullptr;
                if (mat.map_kd != "") {
                    int id = load_texture(path.base_name() + "/" + mat.map_kd, tex_map, scene);
                    diff_tex = id >= 0 ? scene.textures[id].get() : nullptr;
                }

                Texture* spec_tex = nullptr;
                if (mat.map_ks != "") {
                    int id = load_texture(path.base_name() + "/" + mat.map_ks, tex_map, scene);
                    spec_tex = id >= 0 ? scene.textures[id].get() : nullptr;
                }

                auto kd = dot(mat.kd, luminance);
                auto ks = dot(mat.ks, luminance);
                Bsdf* diff = nullptr, *spec = nullptr;

                if (ks > 0 || spec_tex) {
                    if (!spec_tex) {
                        scene.textures.emplace_back(new ConstantTexture(mat.ks));
                        spec_tex = scene.textures.back().get();
                    }
                    spec = new GlossyPhongBsdf(*spec_tex, mat.ns);
                    ks = ks == 0 ? 1.0f : ks;
                }

                if (kd > 0 || diff_tex) {
                    if (!diff_tex) {
                        scene.textures.emplace_back(new ConstantTexture(mat.kd));
                        diff_tex = scene.textures.back().get();
                    }
                    diff = new DiffuseBsdf(*diff_tex);
                    kd = kd == 0 ? 1.0f : kd;
                }

                if (spec && diff) {
                    auto k  = ks / (kd + ks);
                    auto ty = k < 0.2f || mat.ns < 10.0f // Approximate threshold
                        ? Bsdf::Type::Diffuse
                        : Bsdf::Type::Glossy;
                    bsdf = new CombineBsdf(ty, diff, spec, k);
                } else {
                    bsdf = diff ? diff : spec;
                }

                break;
        }*/
    }
}

std::unique_ptr<Scene::Node> load(SceneLoader& scene_loader, const std::string_view& file_name) {
    static constexpr bool is_strict = false;

    auto file = parse_obj(std::string(file_name), is_strict);
    MaterialLib material_lib;
    for (auto& mtl_file : file.mtl_files)
        parse_mtl(mtl_file, material_lib, is_strict);

    convert_materials(scene_loader, material_lib, is_strict);
}

} // namespace sol::obj
