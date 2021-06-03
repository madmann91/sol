#include <proto/random.h>

#include "sol/lights.h"
#include "sol/textures.h"
#include "sol/samplers.h"

namespace sol {

LightSample PointLight::sample_area(Sampler&, const proto::Vec3f& from) const {
    return make_sample(pos_, from - pos_, intensity_, 1.0f, proto::uniform_sphere_pdf<float>(), 1.0f);
}

LightSample PointLight::sample_emission(Sampler& sampler) const {
    auto [dir, pdf_dir] = proto::sample_uniform_sphere(sampler(), sampler());
    return make_sample(pos_, dir, intensity_, 1.0f, pdf_dir, 1.0f);
}

EmissionValue PointLight::emission(const proto::Vec3f&, const proto::Vec2f&) const {
    return EmissionValue { Color::black(), 1.0f, 1.0f };
}

LightSample TriangleLight::sample_area(Sampler& sampler, const proto::Vec3f& from) const {
    auto [uv, pos] = sample(sampler);
    auto dir = from - pos;
    auto cos = proto::dot(dir, normal_) / proto::length(dir);
    return make_sample(pos, dir, intensity_(uv), inv_area_, proto::cosine_hemisphere_pdf(cos), cos);
}

LightSample TriangleLight::sample_emission(Sampler& sampler) const {
    auto local = proto::ortho_basis(normal_);
    auto [uv, pos] = sample(sampler);
    auto [dir, pdf_dir] = proto::sample_cosine_hemisphere(sampler(), sampler());
    return make_sample(pos, local * dir, intensity_(uv), inv_area_, pdf_dir, proto::dot(dir, normal_));
}

EmissionValue TriangleLight::emission(const proto::Vec3f& dir, const proto::Vec2f& uv) const {
    auto cos = proto::cosine_hemisphere_pdf(proto::dot(dir, proto::normalize(triangle_.normal())));
    return cos > 0
        ? EmissionValue { intensity_(uv), inv_area_, cos }
        : EmissionValue { Color::black(), 1.0f, 1.0f };
}

std::pair<proto::Vec2f, proto::Vec3f> TriangleLight::sample(Sampler& sampler) const {
    auto uv = proto::Vec2f(sampler(), sampler());
    if (uv[0] + uv[1] > 1)
        uv = proto::Vec2f(1) - uv;
    auto pos = proto::lerp(triangle_.v0, triangle_.v1, triangle_.v2, uv[0], uv[1]);
    return std::pair { uv, pos };
}

} // namespace sol
