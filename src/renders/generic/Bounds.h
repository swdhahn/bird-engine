#ifndef BIRD_BOUNDS_H
#define BIRD_BOUNDS_H

#include <array>
#include <cstddef>
#include <limits>

#include "../../util/Math.h"

namespace bird {

/**
 * Axis-aligned bounding box. A default-constructed box is empty (inverted),
 * so expanding it by any point yields that point.
 */
struct AABB {
	Vector3 min{std::numeric_limits<float>::max()};
	Vector3 max{-std::numeric_limits<float>::max()};

	/** @return True when no point has been added. */
	bool isEmpty() const { return min.x > max.x; }

	/** Grows the box to enclose a point. */
	void expand(const Vector3& point) {
		min = glm::min(min, point);
		max = glm::max(max, point);
	}

	Vector3 center() const { return (min + max) * 0.5f; }
	Vector3 extents() const { return (max - min) * 0.5f; }

	/**
	 * Builds the box enclosing tightly packed xyz positions.
	 * @param positions Pointer to 3 * count floats.
	 * @param count Number of vertices.
	 */
	static AABB fromPositions(const float* positions, size_t count) {
		AABB box;
		for (size_t i = 0; i < count; i++) {
			box.expand(Vector3(positions[i * 3], positions[i * 3 + 1],
							   positions[i * 3 + 2]));
		}
		return box;
	}

	/**
	 * Transforms the box and re-fits an axis-aligned box around the result
	 * (Arvo's method: O(1), no corner enumeration).
	 * @param m Affine transform.
	 */
	AABB transformed(const Matrix4& m) const {
		if (isEmpty()) return *this;
		const Vector3 c = Vector3(m * Vector4(center(), 1.0f));
		const Vector3 e = extents();
		Vector3 newExtents(0.0f);
		for (int row = 0; row < 3; row++) {
			for (int col = 0; col < 3; col++) {
				newExtents[row] += std::abs(m[col][row]) * e[col];
			}
		}
		return AABB{c - newExtents, c + newExtents};
	}
};

/**
 * View frustum as six inward-facing planes (xyz normal, w distance), with
 * conservative AABB and sphere rejection.
 */
struct Frustum {
	std::array<Vector4, 6> planes;

	/**
	 * Extracts planes from a combined projection * view matrix
	 * (Gribb/Hartmann), for an OpenGL [-1, 1] clip-space depth range.
	 */
	static Frustum fromViewProjection(const Matrix4& viewProjection) {
		const Matrix4 t = glm::transpose(viewProjection);
		Frustum f;
		f.planes[0] = t[3] + t[0];	// left
		f.planes[1] = t[3] - t[0];	// right
		f.planes[2] = t[3] + t[1];	// bottom
		f.planes[3] = t[3] - t[1];	// top
		f.planes[4] = t[3] + t[2];	// near
		f.planes[5] = t[3] - t[2];	// far
		for (Vector4& plane : f.planes) {
			plane /= glm::length(Vector3(plane));
		}
		return f;
	}

	/** @return False only when the box lies entirely outside a plane. */
	bool intersects(const AABB& box) const {
		if (box.isEmpty()) return false;
		const Vector3 c = box.center();
		const Vector3 e = box.extents();
		for (const Vector4& p : planes) {
			const float radius = e.x * std::abs(p.x) + e.y * std::abs(p.y) +
								 e.z * std::abs(p.z);
			if (glm::dot(Vector3(p), c) + p.w < -radius) return false;
		}
		return true;
	}

	/** @return False only when the sphere lies entirely outside a plane. */
	bool intersectsSphere(const Vector3& center, float radius) const {
		for (const Vector4& p : planes) {
			if (glm::dot(Vector3(p), center) + p.w < -radius) return false;
		}
		return true;
	}
};

}  // namespace bird

#endif	// BIRD_BOUNDS_H
