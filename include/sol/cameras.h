#ifndef SOL_CAMERAS_H
#define SOL_CAMERAS_H

#include <numbers>

#include <proto/vec.h>
#include <proto/ray.h>

namespace sol {

/// Structure that holds the local geometry information on a camera lens.
struct LensGeometry {
    float cos;	///< Cosine between the local camera direction and the image plane normal
    float dist; ///< Distance between the camera and the point on the image plane
    float area; ///< Local pixel area divided by total area
};

/// Base class for cameras.
/// By convention, uv-coordinates on the image plane are in the range `[-1, 1]`.
class Camera {
public:
	virtual ~Camera() {}

	/// Generates a ray for a point on the image plane, represented by uv-coordinates.
	virtual proto::Rayf generate_ray(const proto::Vec2f& uv) const = 0;
	/// Projects a point onto the image plane and returns the corresponding uv-coordinates.
	virtual proto::Vec2f project(const proto::Vec3f& point) const = 0;
	/// Returns a point onto the image plane from uv-coordinates.
	virtual proto::Vec3f unproject(const proto::Vec2f& uv) const = 0;
	/// Returns the lens geometry at a given point on the image plane, represented by its uv-coordinates.
	virtual LensGeometry geometry(const proto::Vec2f& uv) const = 0;
};

/// A perspective camera based on the pinhole camera model.
class PerspectiveCamera final : public Camera {
public:
	PerspectiveCamera(
		const proto::Vec3f& eye,
		const proto::Vec3f& dir,
		const proto::Vec3f& up,
		float horz_fov,
		float ratio);

	proto::Rayf generate_ray(const proto::Vec2f&) const override;
	proto::Vec2f project(const proto::Vec3f&) const override;
	proto::Vec3f unproject(const proto::Vec2f&) const override;
	LensGeometry geometry(const proto::Vec2f&) const override;

private:
	proto::Vec3f eye_;
	proto::Vec3f dir_;
	proto::Vec3f right_;
	proto::Vec3f up_;
	float w_, h_;
};

} // namespace sol

#endif
