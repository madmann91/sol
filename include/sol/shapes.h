#ifndef SOL_SHAPES_H
#define SOL_SHAPES_H

#include <proto/vec.h>
#include <proto/triangle.h>
#include <proto/sphere.h>

namespace sol {

class Sampler;

/// Shape surface sample.
struct ShapeSample {
    proto::Vec2f surf_coords; ///< Surface coordinates of the sample on the surface.
    proto::Vec3f pos;         ///< World position of the sample.
    proto::Vec3f normal;      ///< Surface normal at the sample position on the surface.
    float pdf;                ///< Sampling probability when sampling the area of the shape (in _area_ measure).
};

/// A shape sample obtained by sampling a shape from a point on another surface.
struct DirectionalShapeSample final : ShapeSample {
    /// Sampling probability when sampling the area of the shape from the surface point (in _area_ measure).
    float pdf_from;

    DirectionalShapeSample(const ShapeSample& other)
        : ShapeSample(other), pdf_from(other.pdf)
    {}
};

/// Mixin for all samplable shapes.
template <typename Shape, typename Derived>
struct SamplableShape {
    Shape shape;

    SamplableShape(const Shape& shape)
        : shape(shape)
    {}

    /// Samples the surface of the shape from a given point on another surface.
    ShapeSample sample(Sampler& sampler) const;
    DirectionalShapeSample sample(Sampler& sampler, const proto::Vec3f&) const;

    template <typename Hasher>
    Hasher& hash(Hasher& hasher) const { return shape.hash(hasher); }
    bool operator == (const Derived& other) const { return shape == other.shape; }
};

/// A uniformly-sampled triangle.
struct UniformTriangle final : SamplableShape<proto::Trianglef, UniformTriangle> {
    proto::Vec3f normal;
    float inv_area;

    UniformTriangle(const proto::Trianglef& triangle)
        : SamplableShape(triangle), normal(triangle.normal()), inv_area(1.0f / triangle.area())
    {}

    using SamplableShape<proto::Trianglef, UniformTriangle>::sample;
    ShapeSample sample_at(proto::Vec2f) const;
    DirectionalShapeSample sample_at(const proto::Vec2f&, const proto::Vec3f&) const;
};

/// A uniformly-sampled sphere.
struct UniformSphere final : SamplableShape<proto::Spheref, UniformSphere> {
    float inv_area;

    UniformSphere(const proto::Spheref& sphere)
        : SamplableShape(sphere), inv_area(1.0f / sphere.area())
    {}

    using SamplableShape<proto::Spheref, UniformSphere>::sample;
    ShapeSample sample_at(const proto::Vec2f&) const;
    DirectionalShapeSample sample_at(const proto::Vec2f&, const proto::Vec3f&) const;
};

} // namespace sol

#endif
