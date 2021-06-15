#ifndef SOL_BSDFS_H
#define SOL_BSDFS_H

#include <proto/vec.h>
#include <proto/mat.h>

#include "sol/color.h"
#include "sol/scene.h"

namespace sol {

class Sampler;
class Texture;
class ColorTexture;

/// Sample returned by a BSDF, including direction, pdf, and color.
struct BsdfSample {
    proto::Vec3f in_dir;        ///< Sampled direction
    float pdf;                  ///< Probability density function, evaluated for the direction
    float cos;                  ///< Cosine term of the rendering equation
    Color color;                ///< Color of the sample (BSDF value)
};

/// BSDF represented as a black box that can be sampled and evaluated.
class Bsdf {
public:
    /// Classification of BSDF shapes
    enum class Type {
        Diffuse  = 0,   ///< Mostly diffuse, i.e no major features, mostly uniform
        Glossy   = 1,   ///< Mostly glossy, i.e hard for Photon Mapping
        Specular = 2    ///< Purely specular, i.e merging/connections are not possible
    };

    Bsdf(Type type) : type_(type) {}
    virtual ~Bsdf() {}

    /// Returns the type of the BSDF, useful to make sampling decisions.
    Type type() const { return type_; }

    /// Evaluates the material for the given pair of directions and surface point.
    virtual Color eval(
        [[maybe_unused]] const proto::Vec3f& in_dir,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir) const
    {
        return Color::black();
    }

    /// Samples the material given a surface point and an outgoing direction.
    virtual BsdfSample sample(
        [[maybe_unused]] Sampler& sampler,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir,
        [[maybe_unused]] bool is_adjoint = false) const
    {
        return BsdfSample { surf_info.face_normal, 1.0f, 1.0f, Color::black() };
    }

    /// Returns the probability to sample the given input direction (sampled using the sample function).
    virtual float pdf(
        [[maybe_unused]] const proto::Vec3f& in_dir,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir) const
    {
        return 0.0f;
    }

protected:
    /// Utility function to create a `BsdfSample`.
    /// It prevents corner cases that will cause issues (zero pdf, direction parallel/under the surface).
    /// When `ExpectBelowSurface` is true, it expects the direction to be under the surface, otherwise above.
    template <bool ExpectBelowSurface = false>
    static BsdfSample make_sample(
        const proto::Vec3f& in_dir,
        float pdf, float cos,
        const Color& color,
        const SurfaceInfo& surf_info)
    {
        auto below_surface = proto::dot(in_dir, surf_info.face_normal) < 0;
        return pdf > 0 && (below_surface == ExpectBelowSurface)
            ? BsdfSample { in_dir, pdf, cos, color }
            : BsdfSample { in_dir, 1.0f, 1.0f, Color::black() };
    }

    Type type_;
};

/// Purely diffuse (Lambertian) BSDF.
class DiffuseBsdf final : public Bsdf {
public:
    DiffuseBsdf(const ColorTexture& kd)
        : Bsdf(Bsdf::Type::Diffuse), kd_(kd)
    {}

    Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    BsdfSample sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

private:
    const ColorTexture& kd_;
};

/// Specular part of the modified (physically correct) Phong.
class PhongBsdf final : public Bsdf {
public:
    PhongBsdf(const ColorTexture& ks, const Texture& ns)
        : Bsdf(Type::Glossy), ks_(ks), ns_(ns)
    {}

    Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    BsdfSample sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

private:
    static Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&, const Color&, float);
    static float reflect_cosine(const proto::Vec3f&, const proto::Vec3f&, const proto::Vec3f&);
    static float norm_factor(float);

    const ColorTexture& ks_;
	const Texture& ns_;
};

} // namespace sol

#endif
