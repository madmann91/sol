#include "toml_parser.h"

namespace sol {

proto::Vec2f TomlParser::parse_vec2(toml::node_view<const toml::node> node, const proto::Vec2f& default_val) {
    if (auto array = node.as_array(); array) {
        return proto::Vec2f(
            (*array)[0].value_or(default_val[0]),
            (*array)[1].value_or(default_val[1]));
    }
    return default_val;
}

proto::Vec3f TomlParser::parse_vec3(toml::node_view<const toml::node> node, const proto::Vec3f& default_val) {
    if (auto array = node.as_array(); array) {
        return proto::Vec3f(
            (*array)[0].value_or(default_val[0]),
            (*array)[1].value_or(default_val[1]),
            (*array)[2].value_or(default_val[2]));
    }
    return default_val;
}

proto::Vec4f TomlParser::parse_vec4(toml::node_view<const toml::node> node, const proto::Vec4f& default_val) {
    if (auto array = node.as_array(); array) {
        return proto::Vec4f(
            (*array)[0].value_or(default_val[0]),
            (*array)[1].value_or(default_val[1]),
            (*array)[2].value_or(default_val[2]),
            (*array)[3].value_or(default_val[3]));
    }
    return default_val;
}

RgbColor TomlParser::parse_rgb_color(toml::node_view<const toml::node> node, const RgbColor& default_val) {
    if (auto array = node.as_array(); array) {
        return RgbColor(
            (*array)[0].value_or(default_val.r),
            (*array)[1].value_or(default_val.g),
            (*array)[2].value_or(default_val.b));
    }
    return default_val;
}

} // namespace sol
