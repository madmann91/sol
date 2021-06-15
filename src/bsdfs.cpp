#include <numbers>

#include <proto/random.h>

#include "sol/bsdfs.h"
#include "sol/samplers.h"
#include "sol/textures.h"

namespace sol {

// Diffuse BSDF --------------------------------------------------------------------

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

// Phong BSDF ----------------------------------------------------------------------

Color PhongBsdf::eval(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return eval(in_dir, surf_info, out_dir, ks_.sample_color(surf_info.tex_coords), ns_.sample(surf_info.tex_coords));
}

BsdfSample PhongBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto ks = ks_.sample_color(surf_info.tex_coords);
    auto ns = ns_.sample(surf_info.tex_coords);
    auto basis = proto::ortho_basis(reflect(out_dir, surf_info.normal()));
    auto [in_dir, pdf] = proto::sample_cosine_power_hemisphere(ns, sampler(), sampler());
    auto local_in_dir = surf_info.local * in_dir;
    return BsdfSample { local_in_dir, pdf, in_dir[2], eval(local_in_dir, surf_info, out_dir, ks, ns) };
}

float PhongBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir) const {
    return proto::cosine_power_hemisphere_pdf(reflect_cosine(in_dir, surf_info.normal(), out_dir), ns_.sample(surf_info.tex_coords));
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

} // namespace sol
