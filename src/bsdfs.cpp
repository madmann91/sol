#include <numbers>

#include <proto/random.h>

#include "sol/bsdfs.h"
#include "sol/samplers.h"
#include "sol/textures.h"

namespace sol {

static float reflect_cosine(
    const proto::Vec3f& in_dir,
    const proto::Vec3f& normal,
    const proto::Vec3f& out_dir)
{
    // The minus sign and negative dot are here because `out_dir`
    // is going out of the surface by convention.
    return -proto::negative_dot(in_dir, proto::reflect(out_dir, normal));
}

static float fresnel_factor(float eta, float cos_i, float cos_t) {
    // Evaluates the Fresnel factor given the ratio between two different media
    // and the given cosines for the incoming/transmitted rays.
    auto rs = (eta * cos_i - cos_t) / (eta * cos_i + cos_t);
    auto rp = (cos_i - eta * cos_t) / (cos_i + eta * cos_t);
    return (rs * rs + rp * rp) * 0.5f;
}

// Diffuse BSDF --------------------------------------------------------------------

DiffuseBsdf::DiffuseBsdf(const ColorTexture& kd)
    : Bsdf(Tag::DiffuseBsdf, Type::Diffuse), kd_(kd)
{}

Color DiffuseBsdf::eval(const proto::Vec3f&, const SurfaceInfo& surf_info, const proto::Vec3f&) const {
    return kd_.sample_color(surf_info.tex_coords) * std::numbers::inv_pi_v<float>;
}

std::optional<BsdfSample> DiffuseBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto [in_dir, pdf] = proto::sample_cosine_hemisphere(sampler(), sampler());
    auto local_in_dir = surf_info.local * in_dir;
    return validate_sample(surf_info, BsdfSample {
        .in_dir = local_in_dir,
        .pdf    = pdf,
        .cos    = in_dir[2],
        .color  = eval(local_in_dir, surf_info, out_dir)
    });
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

std::optional<BsdfSample> PhongBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto ks = ks_.sample_color(surf_info.tex_coords);
    auto ns = ns_.sample(surf_info.tex_coords);
    auto basis = proto::ortho_basis(proto::reflect(-out_dir, surf_info.normal()));
    auto [in_dir, pdf] = proto::sample_cosine_power_hemisphere(ns, sampler(), sampler());
    auto local_in_dir = basis * in_dir;
    auto cos = proto::positive_dot(local_in_dir, surf_info.normal());
    return validate_sample(surf_info, BsdfSample {
        .in_dir = local_in_dir,
        .pdf    = pdf,
        .cos    = cos,
        .color  = eval(local_in_dir, surf_info, out_dir, ks, ns)
    });
}

float PhongBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return proto::cosine_power_hemisphere_pdf(ns_.sample(surf_info.tex_coords), reflect_cosine(in_dir, surf_info.normal(), out_dir));
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
    return ks * std::pow(reflect_cosine(in_dir, surf_info.normal(), out_dir), ns) * (ns + 2.0f) * (0.5f * std::numbers::inv_pi_v<float>);
}

// Mirror BSDF ---------------------------------------------------------------------

MirrorBsdf::MirrorBsdf(const ColorTexture& ks)
    : Bsdf(Tag::MirrorBsdf, Type::Specular), ks_(ks)
{}

std::optional<BsdfSample> MirrorBsdf::sample(Sampler&, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    return validate_sample(surf_info, BsdfSample {
        .in_dir = proto::reflect(-out_dir, surf_info.normal()),
        .pdf    = 1.0f,
        .cos    = 1.0f,
        .color  = ks_.sample_color(surf_info.tex_coords)
    });
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

std::optional<BsdfSample> GlassBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool is_adjoint) const {
    auto eta = eta_.sample(surf_info.tex_coords);
    eta = surf_info.is_front_side ? eta : 1.0f / eta;
    auto cos_i = proto::dot(out_dir, surf_info.normal());
    auto cos2_t = 1.0f - eta * eta * (1.0f - cos_i * cos_i);
    if (cos2_t > 0.0f) {
        auto cos_t = std::sqrt(cos2_t);
        if (sampler() > fresnel_factor(eta, cos_i, cos_t)) {
            // Refraction
            auto refract_dir = (eta * cos_i - cos_t) * surf_info.normal() - eta * out_dir;
            auto adjoint_fix = is_adjoint ? eta * eta : 1.0f;
            return validate_sample<true>(surf_info, BsdfSample {
                .in_dir = refract_dir,
                .pdf    = 1.0f,
                .cos    = 1.0f,
                .color  = kt_.sample_color(surf_info.tex_coords) * adjoint_fix
            });
        }
    }

    // Reflection
    return validate_sample(surf_info, BsdfSample {
        .in_dir = proto::reflect(-out_dir, surf_info.normal()),
        .pdf    = 1.0f,
        .cos    = 1.0f,
        .color  = ks_.sample_color(surf_info.tex_coords)
    });
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

// Interpolation BSDF --------------------------------------------------------------

InterpBsdf::InterpBsdf(const Bsdf* a, const Bsdf* b, const Texture& k)
    : Bsdf(Tag::InterpBsdf, infer_type(a->type, b->type)), a_(a), b_(b), k_(k)
{}

RgbColor InterpBsdf::eval(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return lerp(
        a_->eval(in_dir, surf_info, out_dir),
        b_->eval(in_dir, surf_info, out_dir),
        k_.sample(surf_info.tex_coords));
}

std::optional<BsdfSample> InterpBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool is_adjoint) const {
    auto k = k_.sample(surf_info.tex_coords);
    auto target = b_, other = a_;
    if (sampler() > k) {
        std::swap(target, other);
        k = 1.0f - k;
    }
    if (auto sample = target->sample(sampler, surf_info, out_dir, is_adjoint)) {
        sample->pdf   = proto::lerp(other->pdf(sample->in_dir, surf_info, out_dir), sample->pdf, k);
        sample->color = lerp(other->eval(sample->in_dir, surf_info, out_dir), sample->color, k);
        return sample;
    }
    return std::nullopt;
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

Bsdf::Type InterpBsdf::infer_type(Type a, Type b) {
    if (a == Type::Diffuse || b == Type::Diffuse) return Type::Diffuse;
    if (a == Type::Glossy  || b == Type::Glossy ) return Type::Glossy;
    return Type::Specular;
}

} // namespace sol
