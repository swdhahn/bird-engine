#ifndef BIRD_UIGEOMETRY_H
#define BIRD_UIGEOMETRY_H

#include <array>
#include <cmath>
#include <cstdint>

#include "../../util/Math.h"

namespace bird::ui {

/**
 * Corners of the UI unit quad in local space, using a top-left origin with
 * the y axis pointing down. The order (top-left, bottom-left, top-right,
 * bottom-right) matches kQuadIndexPattern.
 */
inline const std::array<Vector2, 4> kUnitQuadCorners{
	Vector2(0.0f, 0.0f), Vector2(0.0f, 1.0f), Vector2(1.0f, 0.0f),
	Vector2(1.0f, 1.0f)};

/** Two triangles over kUnitQuadCorners, relative to the quad's first vertex. */
inline constexpr std::array<uint32_t, 6> kQuadIndexPattern{0, 1, 2, 2, 1, 3};

/**
 * Computes the screen-space corners of a drawable.
 * @param worldMatrix - object-to-screen transform; translation in column 2.
 * @param localOffset - drawable offset within the object's unit space.
 * @param size - drawable extent within the object's unit space.
 * @return the four corners, ordered as kUnitQuadCorners.
 */
inline std::array<Vector2, 4> computeDrawableCorners(
	const Matrix3& worldMatrix, const Vector2& localOffset,
	const Vector2& size) {
	std::array<Vector2, 4> corners{};
	for (size_t i = 0; i < corners.size(); i++) {
		const Vector2 local = localOffset + kUnitQuadCorners[i] * size;
		// Homogeneous z = 1 so that the translation column applies.
		const Vector3 screen = worldMatrix * Vector3(local, 1.0f);
		corners[i] = Vector2(screen.x, screen.y);
	}
	return corners;
}

/**
 * Tests whether a screen-space point lies within an object's unit rectangle.
 * Works for any affine transform, including rotation.
 * @param worldMatrix - object-to-screen transform.
 * @param screenPoint - point in screen pixels.
 * @return true when the point is inside or on the edge; false when the
 *         transform is degenerate (zero scale).
 */
inline bool containsScreenPoint(const Matrix3& worldMatrix,
								const Vector2& screenPoint) {
	constexpr float kDegenerateEpsilon = 1e-12f;
	if (std::abs(glm::determinant(worldMatrix)) < kDegenerateEpsilon) {
		return false;
	}
	const Vector3 local = glm::inverse(worldMatrix) * Vector3(screenPoint, 1.0f);
	return local.x >= 0.0f && local.x <= 1.0f && local.y >= 0.0f &&
		   local.y <= 1.0f;
}

}  // namespace bird::ui

#endif	// !BIRD_UIGEOMETRY_H
