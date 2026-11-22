#ifndef BIRD_BOUNDS_H
#define BIRD_BOUNDS_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

#include "../../util/Math.h"

namespace bird {

/**
 * Axis-aligned bounding box in 3D space.
 *
 * A default-constructed box is empty (inverted: min > max), so expanding it
 * by any point yields a degenerate box enclosing that point. Used for
 * object-space mesh bounds, world-space frustum culling, and particle
 * emitter bounds.
 */
struct AABB {
	Vector3 min{std::numeric_limits<float>::max()};
	Vector3 max{-std::numeric_limits<float>::max()};

	/** @return True when no point has been added to the box. */
	bool isEmpty() const { return min.x > max.x; }

	/**
	 * Grows the box to enclose a point.
	 * @param point World- or local-space point to include.
	 */
	void expand(const Vector3& point) {
		min = glm::min(min, point);
		max = glm::max(max, point);
	}

	/**
	 * Grows the box to enclose another AABB. Empty inputs are ignored.
	 * @param other Box to merge into this one.
	 */
	void expand(const AABB& other) {
		if (other.isEmpty()) return;
		min = glm::min(min, other.min);
		max = glm::max(max, other.max);
	}

	/** @return Geometric center of the box (undefined when empty). */
	Vector3 center() const { return (min + max) * 0.5f; }

	/** @return Half-size along each axis (undefined when empty). */
	Vector3 extents() const { return (max - min) * 0.5f; }

	/** @return Full size (max - min) along each axis (zero when empty). */
	Vector3 size() const { return isEmpty() ? Vector3(0.0f) : (max - min); }

	/**
	 * Builds the box enclosing tightly packed xyz vertex positions.
	 * @param positions Pointer to 3 * count floats.
	 * @param count Number of vertices.
	 * @return Enclosing AABB, or an empty AABB when count is 0.
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
	 * Transforms the box by an affine matrix and re-fits an axis-aligned box
	 * around the result (Arvo's method: O(1), no corner enumeration).
	 * @param m Affine transformation matrix.
	 * @return World-space AABB enclosing the transformed box.
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
 * Bounding sphere in 3D space.
 *
 * A negative radius marks an empty sphere. Useful as a fast rotation-invariant
 * culling volume alongside AABB.
 */
struct BoundingSphere {
	Vector3 center{0.0f};
	/** Radius in space units; negative when empty. */
	float radius = -1.0f;

	/** @return True when the sphere encloses no geometry. */
	bool isEmpty() const { return radius < 0.0f; }

	/**
	 * Builds a sphere circumscribing an AABB.
	 * @param box Source bounding box.
	 * @return Circumscribing sphere, or an empty sphere when box is empty.
	 */
	static BoundingSphere fromAABB(const AABB& box) {
		if (box.isEmpty()) return BoundingSphere{};
		return BoundingSphere{box.center(), glm::length(box.extents())};
	}

	/**
	 * Builds a bounding sphere enclosing tightly packed xyz vertex positions.
	 * Centers the sphere at the AABB midpoint and fits the tightest radius
	 * around all vertices from that center.
	 * @param positions Pointer to 3 * count floats.
	 * @param count Number of vertices.
	 * @return Enclosing bounding sphere, or an empty sphere when count is 0.
	 */
	static BoundingSphere fromPositions(const float* positions, size_t count) {
		if (count == 0 || positions == nullptr) return BoundingSphere{};
		const AABB box = AABB::fromPositions(positions, count);
		const Vector3 c = box.center();
		float maxDistSq = 0.0f;
		for (size_t i = 0; i < count; i++) {
			const Vector3 p(positions[i * 3], positions[i * 3 + 1],
							positions[i * 3 + 2]);
			const Vector3 d = p - c;
			maxDistSq = std::max(maxDistSq, glm::dot(d, d));
		}
		return BoundingSphere{c, std::sqrt(maxDistSq)};
	}

	/**
	 * Transforms the sphere by an affine matrix. Non-uniform scale scales the
	 * radius by the largest basis-axis length to remain conservative.
	 * @param m Affine transformation matrix.
	 * @return Transformed bounding sphere.
	 */
	BoundingSphere transformed(const Matrix4& m) const {
		if (isEmpty()) return *this;
		const Vector3 newCenter = Vector3(m * Vector4(center, 1.0f));
		const float sx = glm::length(Vector3(m[0]));
		const float sy = glm::length(Vector3(m[1]));
		const float sz = glm::length(Vector3(m[2]));
		const float maxScale = std::max(sx, std::max(sy, sz));
		return BoundingSphere{newCenter, radius * maxScale};
	}
};

/** Indices of the six inward-facing planes of a Frustum. */
enum class FrustumPlane : size_t {
	Left = 0,
	Right = 1,
	Bottom = 2,
	Top = 3,
	Near = 4,
	Far = 5,
	Count = 6
};

/**
 * View frustum represented as six inward-facing normalized planes
 * (xyz = unit inward normal, w = signed distance from origin), supporting
 * conservative AABB, sphere, and point containment tests.
 */
struct Frustum {
	std::array<Vector4, static_cast<size_t>(FrustumPlane::Count)> planes{};

	/**
	 * Extracts the six frustum planes from a combined projection * view matrix
	 * using the Gribb/Hartmann method for OpenGL [-1, 1] clip-space depth.
	 * @param viewProjection Combined projection * view matrix.
	 * @return Normalized Frustum.
	 */
	static Frustum fromViewProjection(const Matrix4& viewProjection) {
		const Matrix4 t = glm::transpose(viewProjection);
		Frustum f;
		f.planes[static_cast<size_t>(FrustumPlane::Left)] = t[3] + t[0];
		f.planes[static_cast<size_t>(FrustumPlane::Right)] = t[3] - t[0];
		f.planes[static_cast<size_t>(FrustumPlane::Bottom)] = t[3] + t[1];
		f.planes[static_cast<size_t>(FrustumPlane::Top)] = t[3] - t[1];
		f.planes[static_cast<size_t>(FrustumPlane::Near)] = t[3] + t[2];
		f.planes[static_cast<size_t>(FrustumPlane::Far)] = t[3] - t[2];
		for (Vector4& plane : f.planes) {
			const float len = glm::length(Vector3(plane));
			if (len > 0.0f) plane /= len;
		}
		return f;
	}

	/**
	 * Returns a specific frustum plane equation (xyz normal, w offset).
	 * @param which Plane selector.
	 */
	const Vector4& plane(FrustumPlane which) const {
		return planes[static_cast<size_t>(which)];
	}

	/**
	 * Tests whether a point lies inside or on the boundary of the frustum.
	 * @param point World-space point.
	 * @return True if inside all six planes.
	 */
	bool containsPoint(const Vector3& point) const {
		for (const Vector4& p : planes) {
			if (glm::dot(Vector3(p), point) + p.w < 0.0f) return false;
		}
		return true;
	}

	/**
	 * Conservative AABB intersection test.
	 * @param box World-space bounding box.
	 * @return False only when the box lies completely outside at least one
	 *         plane (or when the box is empty).
	 */
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

	/**
	 * Conservative bounding-sphere intersection test.
	 * @param sphere World-space bounding sphere.
	 * @return False only when the sphere lies completely outside at least one
	 *         plane (or when the sphere is empty).
	 */
	bool intersects(const BoundingSphere& sphere) const {
		if (sphere.isEmpty()) return false;
		return intersectsSphere(sphere.center, sphere.radius);
	}

	/**
	 * Conservative sphere intersection test.
	 * @param center World-space sphere center.
	 * @param radius Sphere radius (>= 0).
	 * @return False only when the sphere lies entirely outside a plane.
	 */
	bool intersectsSphere(const Vector3& center, float radius) const {
		for (const Vector4& p : planes) {
			if (glm::dot(Vector3(p), center) + p.w < -radius) return false;
		}
		return true;
	}
};

}  // namespace bird

#endif	// BIRD_BOUNDS_H
