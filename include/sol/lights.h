#ifndef SOL_LIGHTS_H
#define SOL_LIGHTS_H

#include <proto/vec.h>
#include <proto/triangle.h>

#include "sol/color.h"

namespace sol {

class Sampler;
class Texture;

/// Result from sampling the area of a light source.
struct LightSample {
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
    float pdf_area;     ///< Probability to sample the point on the light
    float pdf_dir;      ///< Probability to sample the direction
};

class Light {
public:
    virtual ~Light() {}

    /// Samples the area of a light source from the given point on another surface.
    virtual LightSample sample_area(Sampler&, const proto::Vec3f& from) const;
    /// Samples the emissive surface of the light.
    virtual LightSample sample_emission(Sampler&) const;
    /// Computes the emission value of this light, for a given point on the light, and a given direction.
    /// The direction should be oriented outwards (from the light _to_ the surface).
    virtual EmissionValue emission(const proto::Vec3f& dir, const proto::Vec2f& uv) const;

    /// Returns true if this light has an area.
    virtual bool has_area() const = 0;

protected:
    /// Utility function to create a `LightSample`.
    /// Just like its counterpart for `BsdfSample`, this prevents corner cases for pdfs or cosines.
    static LightSample make_sample(
        const proto::Vec3f& pos,
        const proto::Vec3f& dir,
        const Color& intensity,
        float pdf_area, float pdf_dir, float cos)
    {
        return pdf_area > 0 && pdf_dir > 0 && cos > 0
            ? LightSample { pos, dir, intensity, pdf_area, pdf_dir, cos }
            : LightSample { pos, dir, Color::black(), 1.0f, 1.0f, 1.0f };
    }
};

/// A single-point light.
class PointLight final : public Light {
public:
    PointLight(const proto::Vec3f& pos, const Color& intensity)
        : pos_(pos), intensity_(intensity)
    {}

    LightSample sample_area(Sampler&, const proto::Vec3f&) const override;
    LightSample sample_emission(Sampler&) const override;
    EmissionValue emission(const proto::Vec3f&, const proto::Vec2f&) const override;

    bool has_area() const override { return false; }

private:
    proto::Vec3f pos_;
    Color intensity_;
};

/// A light in the shape of a triangle.
class TriangleLight final : public Light {
public:
    TriangleLight(const proto::Trianglef& triangle, const Texture& intensity)
        : triangle_(triangle), intensity_(intensity)
    {
        normal_ = proto::normalize(triangle_.normal());
        inv_area_ = 1.0f / triangle_.area();
    }

    LightSample sample_area(Sampler&, const proto::Vec3f&) const override;
    LightSample sample_emission(Sampler&) const override;
    EmissionValue emission(const proto::Vec3f&, const proto::Vec2f&) const override;

    bool has_area() const override { return true; }

private:
    std::pair<proto::Vec2f, proto::Vec3f> sample(Sampler&) const;

    proto::Trianglef triangle_;
    const Texture& intensity_;
    proto::Vec3f normal_;
    float inv_area_;
};

} // namespace sol

#endif
