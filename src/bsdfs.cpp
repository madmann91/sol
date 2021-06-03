#include <numbers>

#include <proto/random.h>

#include "sol/bsdfs.h"
#include "sol/samplers.h"
#include "sol/textures.h"

namespace sol {

Color DiffuseBsdf::eval(const proto::Vec3f&, const SurfaceInfo& surf_info, const proto::Vec3f&) const {
    return kd_(surf_info.uv) * std::numbers::inv_pi_v<float>;
}

BsdfSample DiffuseBsdf::sample(Sampler& sampler, const SurfaceInfo& surf_info, const proto::Vec3f& out_dir, bool) const {
    auto [in_dir, pdf] = proto::sample_cosine_hemisphere(sampler(), sampler());
    return BsdfSample { surf_info.local * in_dir, pdf, in_dir[2], eval(in_dir, surf_info, out_dir) };
}

float DiffuseBsdf::pdf(const proto::Vec3f& in_dir, const SurfaceInfo& surf_info, const proto::Vec3f&) const {
    return proto::cosine_hemisphere_pdf(std::max(proto::dot(in_dir, surf_info.normal()), 0.0f));
}

} // namespace sol
