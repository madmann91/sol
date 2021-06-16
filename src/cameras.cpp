#include "sol/cameras.h"

namespace sol {

PerspectiveCamera::PerspectiveCamera(
    const proto::Vec3f& eye,
    const proto::Vec3f& dir,
    const proto::Vec3f& up,
    float horz_fov,
    float aspect_ratio)
    : eye_(eye)
{
    dir_ = proto::normalize(dir);
    right_ = proto::normalize(proto::cross(dir, up));
    up_ = proto::cross(right_, dir_);

    w_ = std::tan(horz_fov * std::numbers::pi_v<float> / 360.0f);
    h_ = w_ / aspect_ratio;
    right_ *= w_;
    up_ *= h_;
}

proto::Rayf PerspectiveCamera::generate_ray(const proto::Vec2f& uv) const {
    return proto::Rayf(eye_, proto::normalize(dir_ + uv[0] * right_ + uv[1] * up_));
}

proto::Vec2f PerspectiveCamera::project(const proto::Vec3f& point) const {
    auto d = proto::normalize(point - eye_);
    return proto::Vec2f(dot(d, right_) / (w_ * w_), dot(d, up_) / (h_ * h_));
}

proto::Vec3f PerspectiveCamera::unproject(const proto::Vec2f& uv) const {
    return eye_ + dir_ + uv[0] * right_ + uv[1] * up_;
}

LensGeometry PerspectiveCamera::geometry(const proto::Vec2f& uv) const {
    auto d = std::sqrt(1.0f + uv[0] * uv[0] * w_ * w_ + uv[1] * uv[1] * h_ * h_);
    return LensGeometry { 1.0f / d, d, 1.0f / (4.0f * w_ * h_) };
}

} // namespace sol
