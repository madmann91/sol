#include <proto/random.h>

#include "sol/lights.h"
#include "sol/textures.h"
#include "sol/samplers.h"

namespace sol {

// Point Light ---------------------------------------------------------------------

PointLight::PointLight(const proto::Vec3f& pos, const Color& intensity)
    : Light(Tag::PointLight), pos_(pos), intensity_(intensity)
{}

std::optional<LightAreaSample> PointLight::sample_area(Sampler&, const proto::Vec3f&) const {
    return std::make_optional(LightAreaSample {
        .pos       = pos_,
        .intensity = intensity_,
        .pdf_from  = 1.0f,
        .pdf_area  = 1.0f,
        .pdf_dir   = proto::uniform_sphere_pdf<float>(),
        .cos       = 1.0f
    });
}

std::optional<LightEmissionSample> PointLight::sample_emission(Sampler& sampler) const {
    auto [dir, pdf_dir] = proto::sample_uniform_sphere(sampler(), sampler());
    return std::make_optional(LightEmissionSample {
        .pos       = pos_,
        .dir       = dir,
        .intensity = intensity_,
        .pdf_area  = 1.0f,
        .pdf_dir   = pdf_dir,
        .cos       = 1.0f
    });
}

EmissionValue PointLight::emission(const proto::Vec3f&, const proto::Vec3f&, const proto::Vec2f&) const {
    return EmissionValue { Color::black(), 1.0f, 1.0f, 1.0f };
}

float PointLight::pdf_from(const proto::Vec3f&, const proto::Vec2f&) const {
    return 1.0f;
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

// Area Light ----------------------------------------------------------------------

template <typename Shape>
AreaLight<Shape>::AreaLight(const Shape& shape, const ColorTexture& intensity)
    : Light(Tag::AreaLight), shape_(shape), intensity_(intensity)
{}

template <typename Shape>
std::optional<LightAreaSample> AreaLight<Shape>::sample_area(Sampler& sampler, const proto::Vec3f& from) const {
    auto sample = shape_.sample(sampler, from);
    auto dir = proto::normalize(from - sample.pos);
    auto cos = proto::positive_dot(dir, sample.normal);
    return cos > 0 ? std::make_optional(LightAreaSample {
        .pos       = sample.pos,
        .intensity = intensity_.sample_color(sample.surf_coords),
        .pdf_from  = sample.pdf_from,
        .pdf_area  = sample.pdf,
        .pdf_dir   = proto::cosine_hemisphere_pdf(cos),
        .cos       = cos
    }) : std::nullopt;
}

template <typename Shape>
std::optional<LightEmissionSample> AreaLight<Shape>::sample_emission(Sampler& sampler) const {
    auto sample = shape_.sample(sampler);
    auto [dir, pdf_dir] = proto::sample_cosine_hemisphere(sampler(), sampler());
    auto cos = dir[2];
    return cos > 0 ? std::make_optional(LightEmissionSample {
        .pos       = sample.pos,
        .dir       = proto::ortho_basis(sample.normal) * dir,
        .intensity = intensity_.sample_color(sample.surf_coords),
        .pdf_area  = sample.pdf,
        .pdf_dir   = pdf_dir,
        .cos       = cos
    }) : std::nullopt;
}

template <typename Shape>
EmissionValue AreaLight<Shape>::emission(const proto::Vec3f& from, const proto::Vec3f& dir, const proto::Vec2f& uv) const {
    auto sample = shape_.sample_at(uv, from);
    auto cos = proto::dot(dir, sample.normal);
    if (cos <= 0)
        return EmissionValue { Color::black(), 1.0f, 1.0f, 1.0f };
    return EmissionValue {
        .intensity = intensity_.sample_color(uv),
        .pdf_from  = sample.pdf_from,
        .pdf_area  = sample.pdf,
        .pdf_dir   = proto::cosine_hemisphere_pdf(cos)
    };
}

template <typename Shape>
float AreaLight<Shape>::pdf_from(const proto::Vec3f& from, const proto::Vec2f& uv) const {
    return shape_.sample_at(uv, from).pdf_from;
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

template class AreaLight<UniformTriangle>;
template class AreaLight<UniformSphere>;

} // namespace sol
