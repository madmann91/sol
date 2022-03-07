#ifndef SOL_BSDFS_H
#define SOL_BSDFS_H

#include <optional>

#include <proto/vec.h>
#include <proto/mat.h>
#include <proto/hash.h>

#include "sol/color.h"
#include "sol/geometry.h"

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
    const enum class Tag {
        DiffuseBsdf,
        PhongBsdf,
        MirrorBsdf,
        GlassBsdf,
        InterpBsdf
    } tag;

    /// Classification of BSDF shapes
    const enum class Type {
        Diffuse  = 0,   ///< Mostly diffuse, i.e no major features, mostly uniform
        Glossy   = 1,   ///< Mostly glossy, i.e hard for Photon Mapping
        Specular = 2    ///< Purely specular, i.e merging/connections are not possible
    } type;

    Bsdf(Tag tag, Type type)
        : tag(tag), type(type)
    {}

    virtual ~Bsdf() {}

    /// Evaluates the material for the given pair of directions and surface point.
    virtual Color eval(
        [[maybe_unused]] const proto::Vec3f& in_dir,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir) const
    {
        return Color::black();
    }

    /// Samples the material given a surface point and an outgoing direction.
    /// This may fail to return a sample, for instance if random sampling generated an
    /// incorrect direction, or if the surface configuration makes it impossible to generate
    /// a proper direction.
    virtual std::optional<BsdfSample> sample(
        [[maybe_unused]] Sampler& sampler,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir,
        [[maybe_unused]] bool is_adjoint = false) const
    {
        return std::nullopt;
    }

    /// Returns the probability to sample the given input direction (sampled using the sample function).
    virtual float pdf(
        [[maybe_unused]] const proto::Vec3f& in_dir,
        [[maybe_unused]] const SurfaceInfo& surf_info,
        [[maybe_unused]] const proto::Vec3f& out_dir) const
    {
        return 0.0f;
    }

    virtual proto::fnv::Hasher& hash(proto::fnv::Hasher&) const = 0;
    virtual bool equals(const Bsdf&) const = 0;

protected:
    // Utility function to check the validity of a `BsdfSample`.
    // It prevents corner cases that will cause issues (zero pdf, direction parallel/under the surface).
    // When `ExpectBelowSurface` is true, it expects the direction to be under the surface, otherwise above.
    template <bool ExpectBelowSurface = false>
    static std::optional<BsdfSample> validate_sample(const SurfaceInfo& surf_info, const BsdfSample& bsdf_sample) {
        bool is_below_surface = proto::dot(bsdf_sample.in_dir, surf_info.face_normal) < 0;
        return bsdf_sample.pdf > 0 && (is_below_surface == ExpectBelowSurface)
            ? std::make_optional(bsdf_sample) : std::nullopt;
    }
};

/// Purely diffuse (Lambertian) BSDF.
class DiffuseBsdf final : public Bsdf {
public:
    DiffuseBsdf(const ColorTexture&);

    std::optional<BsdfSample> sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Bsdf&) const override;

private:
    const ColorTexture& kd_;
};

/// Specular part of the modified (physically correct) Phong.
class PhongBsdf final : public Bsdf {
public:
    PhongBsdf(const ColorTexture&, const Texture&);

    std::optional<BsdfSample> sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Bsdf&) const override;

private:
    static Color eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&, const Color&, float);

    const ColorTexture& ks_;
	const Texture& ns_;
};

/// Perfect mirror BSDF.
class MirrorBsdf final : public Bsdf {
public:
    MirrorBsdf(const ColorTexture&);

    std::optional<BsdfSample> sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Bsdf&) const override;

private:
    const ColorTexture& ks_;
};

/// BSDF that can represent glass or any separation between two mediums with different indices.
class GlassBsdf final : public Bsdf {
public:
    GlassBsdf(
        const ColorTexture& ks,
        const ColorTexture& kt,
        const Texture& eta);

    std::optional<BsdfSample> sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Bsdf&) const override;

private:
    const ColorTexture& ks_;
    const ColorTexture& kt_;
    const Texture& eta_;
};

/// A BSDF that interpolates between two Bsdfs.
class InterpBsdf final : public Bsdf {
public:
    InterpBsdf(const Bsdf*, const Bsdf*, const Texture&);

    std::optional<BsdfSample> sample(Sampler&, const SurfaceInfo&, const proto::Vec3f&, bool) const override;
    RgbColor eval(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;
    float pdf(const proto::Vec3f&, const SurfaceInfo&, const proto::Vec3f&) const override;

    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Bsdf&) const override;

private:
    static Type infer_type(Type, Type);

    const Bsdf* a_;
    const Bsdf* b_;
    const Texture& k_;
};

} // namespace sol

#endif
