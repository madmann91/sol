#ifndef SOL_TOML_PARSER_H
#define SOL_TOML_PARSER_H

#include <toml++/toml.h>
#include <proto/vec.h>

#include <sol/color.h>

namespace sol {

/// Helper functions to parse TOML data.
struct TomlParser  {
    static proto::Vec2f parse_vec2(toml::node_view<const toml::node>, const proto::Vec2f& = {});
    static proto::Vec3f parse_vec3(toml::node_view<const toml::node>, const proto::Vec3f& = {});
    static proto::Vec4f parse_vec4(toml::node_view<const toml::node>, const proto::Vec4f& = {});
    static RgbColor parse_rgb_color(toml::node_view<const toml::node>, const RgbColor& = {});
};

} // namespace sol

#endif
