#ifndef BIRD_SHADOW_MATH_H
#define BIRD_SHADOW_MATH_H

#include <array>
#include <cstdint>
#include <vector>

#include "../../util/Math.h"

/**
 * Pure CPU shadow-projection math shared by the shadow pass and its tests.
 * All matrices target OpenGL clip space (depth in [-1, 1]).
 */
namespace bird::shadow {

/** Number of cascades rendered for the shadowed directional light. */
constexpr uint32_t CASCADE_COUNT = 4;

/** Point-light cube faces in shader order: +X, -X, +Y, -Y, +Z, -Z. */
constexpr uint32_t CUBE_FACE_COUNT = 6;

/**
 * Practical split scheme (Zhang et al.): blends logarithmic and uniform
 * partitions of [nearPlane, farPlane].
 * @param nearPlane Camera near distance (> 0).
 * @param farPlane Shadow distance (> nearPlane).
 * @param cascadeCount Number of slices (>= 1).
 * @param lambda 0 = uniform, 1 = logarithmic.
 * @return cascadeCount + 1 increasing distances; front = near, back = far.
 * @throws std::invalid_argument on invalid ranges.
 */
std::vector<float> computeCascadeSplits(float nearPlane, float farPlane,
										uint32_t cascadeCount, float lambda);

/** Light-space projection for one cascade. */
struct CascadeProjection {
	Matrix4 viewProjection;
	/** World-space size of one shadow texel. */
	float texelWorldSize;
};

/**
 * Fits an orthographic projection around the bounding sphere of one
 * camera-frustum slice, snapped to whole texels so that the shadow does not
 * shimmer when the camera moves or rotates.
 * @param inverseView Camera world transform.
 * @param fovY Vertical field of view in radians.
 * @param aspect Width / height.
 * @param sliceNear Slice start distance along view direction.
 * @param sliceFar Slice end distance along view direction.
 * @param lightDirection Normalized direction the light travels.
 * @param resolution Cascade tile resolution in texels.
 * @param casterPullback Extra distance behind the slice where casters
 *        still project into the cascade.
 */
CascadeProjection fitCascade(const Matrix4& inverseView, float fovY,
							 float aspect, float sliceNear, float sliceFar,
							 const Vector3& lightDirection, uint32_t resolution,
							 float casterPullback);

/**
 * Perspective field of view, in radians, that widens a frustum of the given
 * base angle by guardTexels on every side of a tile, keeping PCF taps near a
 * tile edge inside the tile.
 */
float guardedFieldOfView(float baseFov, uint32_t tileSize, float guardTexels);

/**
 * Spot-light view-projection.
 * @param position Light position.
 * @param direction Normalized emission direction.
 * @param outerAngle Cone half-angle in radians.
 * @param range Light range (far plane).
 * @param tileSize Atlas tile size, for the PCF guard band.
 */
Matrix4 spotViewProjection(const Vector3& position, const Vector3& direction,
						   float outerAngle, float range, uint32_t tileSize);

/**
 * Point-light view-projections for all six faces, in shader order.
 * @param position Light position.
 * @param range Light range (far plane).
 * @param tileSize Atlas tile size, for the PCF guard band.
 */
std::array<Matrix4, CUBE_FACE_COUNT> pointFaceViewProjections(
	const Vector3& position, float range, uint32_t tileSize);

/**
 * Cube face whose frustum contains a direction from the light; mirrors
 * selectCubeFace() in shaders/include/shadows.glsl.
 * @return Face index in [0, 6).
 */
uint32_t selectCubeFace(const Vector3& lightToPoint);

/** Near plane used by spot and point shadow frusta, relative to range. */
float localShadowNearPlane(float range);

}  // namespace bird::shadow

#endif	// BIRD_SHADOW_MATH_H
