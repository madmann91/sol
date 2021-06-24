#include <proto/random.h>

#include "sol/lights.h"
#include "sol/textures.h"
#include "sol/samplers.h"

namespace sol {

// Point Light ---------------------------------------------------------------------

PointLight::PointLight(const proto::Vec3f& pos, const Color& intensity)
    : Light(Tag::PointLight), pos_(pos), intensity_(intensity)
{}

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

proto::fnv::Hasher& PointLight::hash(proto::fnv::Hasher& hasher) const {
    return pos_.hash(intensity_.hash(hasher.combine(tag)));
}

bool PointLight::equals(const Light& other) const {
    return
        other.tag == tag &&
        static_cast<const PointLight&>(other).pos_ == pos_ &&
        static_cast<const PointLight&>(other).intensity_ == intensity_;
}

// Triangle Light ---------------------------------------------------------------------

template <typename Shape>
AreaLight<Shape>::AreaLight(const Shape& shape, const ColorTexture& intensity)
    : Light(Tag::AreaLight), shape_(shape), intensity_(intensity)
{
    inv_area_ = 1.0f / shape_.area();
}

template <typename Shape>
LightSample AreaLight<Shape>::sample_area(Sampler& sampler, const proto::Vec3f& from) const {
    auto [uv, pos, normal] = sample(sampler);
    auto dir = proto::normalize(from - pos);
    auto cos = proto::positive_dot(dir, normal);
    return make_sample(pos, dir, intensity_.sample_color(uv), inv_area_, proto::cosine_hemisphere_pdf(cos), cos);
}

template <typename Shape>
LightSample AreaLight<Shape>::sample_emission(Sampler& sampler) const {
    auto [uv, pos, normal] = sample(sampler);
    auto local = proto::ortho_basis(normal);
    auto [dir, pdf_dir] = proto::sample_cosine_hemisphere(sampler(), sampler());
    return make_sample(pos, local * dir, intensity_.sample_color(uv), inv_area_, pdf_dir, proto::dot(dir, local.col(2)));
}

template <typename Shape>
EmissionValue AreaLight<Shape>::emission(const proto::Vec3f& dir, const proto::Vec2f& uv) const {
    auto pdf_dir = proto::cosine_hemisphere_pdf(proto::dot(dir, std::get<1>(shape_.sample(uv[0], uv[1]))));
    return pdf_dir > 0
        ? EmissionValue { intensity_.sample_color(uv), inv_area_, pdf_dir }
        : EmissionValue { Color::black(), 1.0f, 1.0f };
}

template <typename Shape>
proto::fnv::Hasher& AreaLight<Shape>::hash(proto::fnv::Hasher& hasher) const {
    return shape_.hash(hasher).combine(&intensity_);
}

template <typename Shape>
bool AreaLight<Shape>::equals(const Light& other) const {
    return
        other.tag == tag &&
        static_cast<const AreaLight&>(other).shape_ == shape_ &&
        &static_cast<const AreaLight&>(other).intensity_ == &intensity_;
}

template <typename Shape>
std::tuple<proto::Vec2f, proto::Vec3f, proto::Vec3f> AreaLight<Shape>::sample(Sampler& sampler) const {
    auto uv = proto::Vec2f(sampler(), sampler());
    auto [pos, normal] = shape_.sample(uv[0], uv[1]);
    return std::tuple { uv, pos, normal };
}

template class AreaLight<proto::Trianglef>;
template class AreaLight<proto::Spheref>;

} // namespace sol
