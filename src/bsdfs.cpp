#include <numbers>

#include <proto/random.h>

#include "sol/bsdfs.h"
#include "sol/samplers.h"
#include "sol/textures.h"

namespace sol {

// Diffuse BSDF --------------------------------------------------------------------

DiffuseBsdf::DiffuseBsdf(const ColorTexture& kd)
    : Bsdf(Tag::DiffuseBsdf, Type::Diffuse), kd_(kd)
{}

Color DiffuseBsdf::eval(const proto::Vec3f&, const SurfaceInfo& surf_info, const proto::Vec3f&) const {
    return kd_.sample_color(surf_info.tex_coords) * std::numbers::inv_pi_v<float>;
}

BsdfSample DiffuseBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto [in_dir, pdf] = proto::sample_cosine_hemisphere(sampler(), sampler());
    auto local_in_dir = surf_info.local * in_dir;
    return BsdfSample { local_in_dir, pdf, in_dir[2], eval(local_in_dir, surf_info, out_dir) };
}

float DiffuseBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f&) const {
    return proto::cosine_hemisphere_pdf(proto::positive_dot(in_dir, surf_info.normal()));
}

proto::fnv::Hasher& DiffuseBsdf::hash(proto::fnv::Hasher& hasher) const {
    return hasher.combine(tag).combine(&kd_);
}

bool DiffuseBsdf::equals(const Bsdf& other) const {
    return other.tag == tag && &static_cast<const DiffuseBsdf&>(other).kd_ == &kd_;
}

// Phong BSDF ----------------------------------------------------------------------

PhongBsdf::PhongBsdf(const ColorTexture& ks, const Texture& ns)
    : Bsdf(Tag::PhongBsdf, Type::Glossy), ks_(ks), ns_(ns)
{}

Color PhongBsdf::eval(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return eval(in_dir, surf_info, out_dir, ks_.sample_color(surf_info.tex_coords), ns_.sample(surf_info.tex_coords));
}

BsdfSample PhongBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto ks = ks_.sample_color(surf_info.tex_coords);
    auto ns = ns_.sample(surf_info.tex_coords);
    auto basis = proto::ortho_basis(reflect(out_dir, surf_info.normal()));
    auto [in_dir, pdf] = proto::sample_cosine_power_hemisphere(ns, sampler(), sampler());
    auto local_in_dir = basis * in_dir;
    auto cos = proto::positive_dot(local_in_dir, surf_info.normal());
    return BsdfSample { local_in_dir, pdf, cos, eval(local_in_dir, surf_info, out_dir, ks, ns) };
}

float PhongBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return proto::cosine_power_hemisphere_pdf(reflect_cosine(in_dir, surf_info.normal(), out_dir), ns_.sample(surf_info.tex_coords));
}

proto::fnv::Hasher& PhongBsdf::hash(proto::fnv::Hasher& hasher) const {
    return hasher.combine(tag).combine(&ks_).combine(&ns_);
}

bool PhongBsdf::equals(const Bsdf& other) const {
    return
        other.tag == tag &&
        &static_cast<const PhongBsdf&>(other).ks_ == &ks_ &&
        &static_cast<const PhongBsdf&>(other).ns_ == &ns_;
}

Color PhongBsdf::eval(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, const Color& ks, float ns) {
    return ks * std::pow(reflect_cosine(in_dir, surf_info.normal(), out_dir), ns) * norm_factor(ns);
}

float PhongBsdf::reflect_cosine(
    const proto::Vec3f& in_dir,
    const proto::Vec3f& normal,
    const proto::Vec3f& out_dir)
{
    // The minus sign and negative dot are here because `out_dir`
    // is going out of the surface by convention.
    return -proto::negative_dot(in_dir, proto::reflect(out_dir, normal));
}

float PhongBsdf::norm_factor(float ns) {
    return (ns + 2.0f) * (0.5f * std::numbers::inv_pi_v<float>);
}

// Mirror BSDF ---------------------------------------------------------------------

MirrorBsdf::MirrorBsdf(const ColorTexture& ks)
    : Bsdf(Tag::MirrorBsdf, Type::Specular), ks_(ks)
{}

BsdfSample MirrorBsdf::sample(Sampler&, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    return make_sample(proto::reflect(out_dir, surf_info.normal()), 1.0f, 1.0f, ks_.sample_color(surf_info.tex_coords), surf_info);
}

proto::fnv::Hasher& MirrorBsdf::hash(proto::fnv::Hasher& hasher) const {
    return hasher.combine(tag).combine(&ks_);
}

bool MirrorBsdf::equals(const Bsdf& other) const {
    return other.tag == tag && &static_cast<const MirrorBsdf&>(other).ks_ == &ks_;
}

// Glass BSDF ----------------------------------------------------------------------

GlassBsdf::GlassBsdf(
    const ColorTexture& ks,
    const ColorTexture& kt,
    const Texture& eta)
    : Bsdf(Tag::GlassBsdf, Type::Specular), ks_(ks), kt_(kt), eta_(eta)
{}

BsdfSample GlassBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool is_adjoint) const {
    auto eta = eta_.sample(surf_info.tex_coords);
    eta = surf_info.is_front_side ? eta : 1.0f / eta;
    auto cos_i = proto::dot(out_dir, surf_info.normal());
    auto cos2_t = 1.0f - eta * eta * (1.0f - cos_i * cos_i);
    if (cos2_t > 0.0f) {
        auto cos_t = std::sqrt(cos2_t);
        auto f = fresnel_factor(eta, cos_i, cos_t);
        if (sampler() > f) {
            // Refraction
            auto t = (eta * cos_i - cos_t) * surf_info.normal() - eta * out_dir;
            auto adjoint_fix = is_adjoint ? eta * eta : 1.0f;
            return make_sample<true>(t, 1.0f, 1.0f, kt_.sample_color(surf_info.tex_coords) * adjoint_fix, surf_info);
        }
    }

    // Reflection
    return make_sample(proto::reflect(out_dir, surf_info.normal()), 1.0f, 1.0f, ks_.sample_color(surf_info.tex_coords), surf_info);
}

proto::fnv::Hasher& GlassBsdf::hash(proto::fnv::Hasher& hasher) const {
    return hasher.combine(tag).combine(&ks_).combine(&kt_).combine(&eta_);
}

bool GlassBsdf::equals(const Bsdf& other) const {
    return
        other.tag == tag &&
        &static_cast<const GlassBsdf&>(other).ks_ == &ks_ &&
        &static_cast<const GlassBsdf&>(other).kt_ == &kt_ &&
        &static_cast<const GlassBsdf&>(other).eta_ == &eta_;
}

float GlassBsdf::fresnel_factor(float k, float cos_i, float cos_t) {
    // Evaluates the fresnel factor given the ratio between two different media
    // and the given cosines of the incoming/transmitted rays.
    auto R_s = (k * cos_i - cos_t) / (k * cos_i + cos_t);
    auto R_p = (cos_i - k * cos_t) / (cos_i + k * cos_t);
    return (R_s * R_s + R_p * R_p) * 0.5f;
}

// Interpolation BSDF --------------------------------------------------------------

InterpBsdf::InterpBsdf(const Bsdf* a, const Bsdf* b, const Texture& k)
    : Bsdf(Tag::InterpBsdf, guess_type(a->type, b->type)), a_(a), b_(b), k_(k)
{}

RgbColor InterpBsdf::eval(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return lerp(
        a_->eval(in_dir, surf_info, out_dir),
        b_->eval(in_dir, surf_info, out_dir),
        k_.sample(surf_info.tex_coords));
}

BsdfSample InterpBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool adjoint) const {
    auto k = k_.sample(surf_info.tex_coords);
    if (sampler() < k) {
        auto sample = b_->sample(sampler, surf_info, out_dir, adjoint);
        sample.pdf   = proto::lerp(a_->pdf (sample.in_dir, surf_info, out_dir), sample.pdf, k);
        sample.color = lerp(a_->eval(sample.in_dir, surf_info, out_dir), sample.color, k);
        return sample;
    } else {
        auto sample = a_->sample(sampler, surf_info, out_dir, adjoint);
        sample.pdf   = proto::lerp(sample.pdf, b_->pdf (sample.in_dir, surf_info, out_dir), k);
        sample.color = lerp(sample.color, b_->eval(sample.in_dir, surf_info, out_dir), k);
        return sample;
    }
}

float InterpBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return proto::lerp(
        a_->pdf(in_dir, surf_info, out_dir),
        b_->pdf(in_dir, surf_info, out_dir),
        k_.sample(surf_info.tex_coords));
}

proto::fnv::Hasher& InterpBsdf::hash(proto::fnv::Hasher& hasher) const {
    return hasher.combine(tag).combine(a_).combine(b_).combine(&k_);
}

bool InterpBsdf::equals(const Bsdf& other) const {
    return
        other.tag == tag &&
        static_cast<const InterpBsdf&>(other).a_ == a_ &&
        static_cast<const InterpBsdf&>(other).b_ == b_ &&
        &static_cast<const InterpBsdf&>(other).k_ == &k_;
}

Bsdf::Type InterpBsdf::guess_type(Type a, Type b) {
    if (a == Type::Diffuse || b == Type::Diffuse) return Type::Diffuse;
    if (a == Type::Glossy  || b == Type::Glossy ) return Type::Glossy;
    return Type::Specular;
}

} // namespace sol
