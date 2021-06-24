#ifndef SOL_LIGHTS_H
#define SOL_LIGHTS_H

#include <optional>

#include <proto/vec.h>
#include <proto/hash.h>
#include <proto/triangle.h>
#include <proto/sphere.h>

#include "sol/color.h"
#include "sol/shapes.h"

namespace sol {

class Sampler;
class ColorTexture;

/// Result from sampling the area of a light source from another surface point.
struct LightAreaSample {
    proto::Vec3f pos;   ///< Position on the light source
    Color intensity;    ///< Intensity along the direction
    float pdf_from;     ///< Probability to sample the point on the light from another point
    float pdf_area;     ///< Probability to sample the point on the light
    float pdf_dir;      ///< Probability to sample the direction between the light source and the surface
    float cos;          ///< Cosine between the direction and the light source geometry
};

/// Result from sampling a light source to get a point and a direction.
struct LightEmissionSample {
    proto::Vec3f pos;   ///< Position on the light source
    proto::Vec3f dir;   ///< Direction of the ray going outwards from the light
    Color intensity;    ///< Intensity along the direction
    float pdf_area;     ///< Probability to sample the point on the light
    float pdf_dir;      ///< Probability to sample the direction
    float cos;          ///< Cosine between the direction and the light source geometry
};

/// Emission value for a given point on the surface of the light, and a given direction.
struct EmissionValue {
    Color intensity;    ///< Intensity of the light source at the given point
    float pdf_from;     ///< Probability to sample the point on the light from another point
    float pdf_area;     ///< Probability to sample the point on the light
    float pdf_dir;      ///< Probability to sample the direction
};

class Light {
public:
    const enum class Tag {
        PointLight,
        AreaLight
    } tag;

    Light(Tag tag)
        : tag(tag)
    {}

    virtual ~Light() {}

    /// Samples the area of a light source from the given point on another surface.
    virtual std::optional<LightAreaSample> sample_area(Sampler&, const proto::Vec3f& from) const = 0;

    /// Samples the emissive surface of the light.
    virtual std::optional<LightEmissionSample> sample_emission(Sampler&) const = 0;

    /// Computes the emission value of this light, from a given point on a surface,
    /// for a given point on the light, and a given direction.
    /// The direction should be oriented outwards (from the light _to_ the surface).
    virtual EmissionValue emission(const proto::Vec3f& from, const proto::Vec3f& dir, const proto::Vec2f& uv) const = 0;

    /// Returns the probability to sample the given point on the light source from another point on a surface.
    virtual float pdf_from(const proto::Vec3f& from, const proto::Vec2f& uv) const = 0;

    /// Returns true if the light source has an area (i.e. it can be hit when intersecting a ray with the scene).
    virtual bool has_area() const = 0;

    virtual proto::fnv::Hasher& hash(proto::fnv::Hasher&) const = 0;
    virtual bool equals(const Light&) const = 0;

protected:
    // Utility function to create a `LightSample`.
    // Just like its counterpart for `BsdfSample`, this prevents corner cases for pdfs or cosines.
    template <typename LightSample>
    static std::optional<LightSample> make_sample(const LightSample& light_sample) {
        return light_sample.pdf_area > 0 && light_sample.pdf_dir > 0 && light_sample.cos > 0
            ? std::make_optional(light_sample) : std::nullopt;
    }
};

/// A single-point light.
class PointLight final : public Light {
public:
    PointLight(const proto::Vec3f&, const Color&);

    std::optional<LightAreaSample> sample_area(Sampler&, const proto::Vec3f&) const override;
    std::optional<LightEmissionSample> sample_emission(Sampler&) const override;
    EmissionValue emission(const proto::Vec3f&, const proto::Vec3f&, const proto::Vec2f&) const override;
    float pdf_from(const proto::Vec3f&, const proto::Vec2f&) const override;

    bool has_area() const override { return false; }

    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Light&) const override;

private:
    proto::Vec3f pos_;
    Color intensity_;
};

/// An area light in the shape of an object given as parameter.
/// The light emission profile is diffuse (i.e. follows the cosine
/// between the normal of the light surface and the emission direction).
template <typename SamplableShape>
class AreaLight final : public Light {
public:
    AreaLight(const SamplableShape&, const ColorTexture&);

    std::optional<LightAreaSample> sample_area(Sampler&, const proto::Vec3f&) const override;
    std::optional<LightEmissionSample> sample_emission(Sampler&) const override;
    EmissionValue emission(const proto::Vec3f&, const proto::Vec3f&, const proto::Vec2f&) const override;
    float pdf_from(const proto::Vec3f&, const proto::Vec2f&) const override;

    bool has_area() const override { return true; }

    proto::fnv::Hasher& hash(proto::fnv::Hasher&) const override;
    bool equals(const Light&) const override;

private:
    SamplableShape shape_;
    const ColorTexture& intensity_;
};

using UniformTriangleLight = AreaLight<UniformTriangle>;
using UniformSphereLight   = AreaLight<UniformSphere>;

} // namespace sol

#endif
