#include "sol/shapes.h"
#include "sol/samplers.h"

namespace sol {

template <typename Shape, typename Derived>
ShapeSample SamplableShape<Shape, Derived>::sample(Sampler& sampler) const {
    auto uv = proto::Vec2f(sampler(), sampler());
    return static_cast<const Derived*>(this)->sample_at(uv);
}

template <typename Shape, typename Derived>
DirectionalShapeSample SamplableShape<Shape, Derived>::sample(Sampler& sampler, const proto::Vec3f& from) const {
    auto uv = proto::Vec2f(sampler(), sampler());
    return static_cast<const Derived*>(this)->sample_at(uv, from);
}

ShapeSample UniformTriangle::sample_at(proto::Vec2f uv) const {
    if (uv[0] + uv[1] > 1)
        uv = proto::Vec2f(1) - uv;
    auto pos = proto::lerp(shape.v0, shape.v1, shape.v2, uv[0], uv[1]);
    return ShapeSample { uv, pos, normal, inv_area };
}

DirectionalShapeSample UniformTriangle::sample_at(const proto::Vec2f& uv, const proto::Vec3f&) const {
    return sample_at(uv);
}

ShapeSample UniformSphere::sample_at(const proto::Vec2f& uv) const {
    auto dir = std::get<0>(proto::sample_uniform_sphere(uv[0], uv[1]));
    auto pos = shape.center + dir * shape.radius;
    return ShapeSample { uv, pos, dir, inv_area };
}

DirectionalShapeSample UniformSphere::sample_at(const proto::Vec2f& uv, const proto::Vec3f&) const {
    return sample_at(uv);
}

template struct SamplableShape<proto::Trianglef, UniformTriangle>;
template struct SamplableShape<proto::Spheref,   UniformSphere>;

} // namespace sol
