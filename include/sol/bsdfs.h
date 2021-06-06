#ifndef SOL_BSDFS_H
#define SOL_BSDFS_H

#include <proto/vec.h>
#include <proto/mat.h>

#include "sol/color.h"

namespace sol {

class Sampler;
class Texture;

/// Sample returned by a BSDF, including direction, pdf, and color.
struct BsdfSample {
    proto::Vec3f in_dir;        ///< Sampled direction
    float pdf;                  ///< Probability density function, evaluated for the direction
    float cos;                  ///< Cosine term of the rendering equation
    Color color;                ///< Color of the sample (BSDF value)
};

/// Surface information for a specific point on a surface.
/// This information is required to perform various shading operations.
struct SurfaceInfo {
    bool is_front_side;         ///< True if the point is on the front of the surface
    proto::Vec3f point;         ///< Hit point in world coordinates
    proto::Vec2f tex_coords;    ///< Texture coordinates
    proto::Vec2f surf_coords;   ///< Coordinates on the surface (depends on the surface type)
    proto::Vec3f face_normal;   ///< Geometric normal
    proto::Mat3x3f local;       ///< Local coordinates at the hit point, w.r.t shading normal

    proto::Vec3f normal() const { return local.col(2); }
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
    DiffuseBsdf(const Texture& kd)
        : Bsdf(Bsdf::Type::Diffuse), kd_(kd)
    {}

    Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    BsdfSample sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

private:
    const Texture& kd_;
};

} // namespace sol

#endif
