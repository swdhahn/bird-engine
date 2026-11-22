#ifndef BIRD_EDITOR_VIEWPORT_PICKER_H
#define BIRD_EDITOR_VIEWPORT_PICKER_H

#include "../renders/generic/Bounds.h"
#include "../scene/Camera.h"
#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../util/Math.h"

namespace bird::editor {

/**
 * World-space half-line defined by an origin and a unit direction vector.
 */
struct Ray {
	Vector3 origin{0.0f};
	Vector3 direction{0.0f, 0.0f, -1.0f};

	/** Evaluates a point along the ray at parameter @p t. */
	Vector3 at(float t) const { return origin + direction * t; }
};

/**
 * Result of a 3D viewport entity raycast pick.
 */
struct PickHit {
	/** Closest intersected entity, or nullptr if nothing was hit. */
	WorldObject* entity = nullptr;
	/** Distance from the ray origin to the entry intersection point. */
	float distance = 0.0f;
	/** World-space intersection point. */
	Vector3 point{0.0f};

	explicit operator bool() const { return entity != nullptr; }
};

/**
 * Stateless raycasting utilities for selecting 3D entities from screen clicks.
 *
 * Unprojects window-space mouse coordinates through the active Camera's
 * inverse view-projection matrix and tests against world-space AABBs derived
 * from each entity's MeshComponent bounds (or a fallback pick box for
 * non-mesh entities such as lights, cameras, and empty nodes).
 */
class ViewportPicker {
   public:
	/** Default half-extent of the pick box for entities without meshes. */
	static constexpr float kDefaultPickHalfExtent = 0.35f;

	/**
	 * Unprojects a 2D window coordinate into a world-space Ray.
	 *
	 * @param screenPos Cursor position in window pixels (origin top-left).
	 * @param viewportSize Window/viewport dimensions in pixels (must be > 0).
	 * @param camera Active Camera providing view and perspective matrices.
	 * @return Normalized world-space Ray originating on the near plane.
	 */
	static Ray screenToRay(Vector2 screenPos, Vector2 viewportSize,
						   const Camera& camera);

	/**
	 * Performs slab-method intersection between a Ray and an axis-aligned box.
	 *
	 * @param ray World-space ray (direction should be normalized).
	 * @param box World-space axis-aligned bounding box.
	 * @param outDistance Receives the non-negative hit distance along @p ray.
	 * @return True if the ray intersects @p box in front of its origin.
	 */
	static bool intersectRayAABB(const Ray& ray, const AABB& box,
								 float& outDistance);

	/**
	 * Computes the world-space AABB for @p entity.
	 *
	 * If the entity has a MeshComponent with valid local bounds, returns the
	 * union of all transformed mesh bounds. Otherwise returns a cube of half-
	 * extent @p defaultHalfExtent centered at the entity's world position.
	 *
	 * @param entity Target entity (must not be null).
	 * @param defaultHalfExtent Half-extent fallback for non-mesh entities.
	 * @return World-space bounding box.
	 */
	static AABB computeEntityBounds(
		WorldObject* entity,
		float defaultHalfExtent = kDefaultPickHalfExtent);

	/**
	 * Casts @p ray against every entity in @p rootScene (recursing into nested
	 * Scene containers) and returns the closest hit.
	 *
	 * @param ray World-space picking ray.
	 * @param rootScene Root scene to search (the root itself is not picked).
	 * @param ignoreEntity Optional entity to exclude (e.g. the editor camera).
	 * @return Closest PickHit, or an empty PickHit if nothing was intersected.
	 */
	static PickHit pick(const Ray& ray, Scene* rootScene,
						const WorldObject* ignoreEntity = nullptr);
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_VIEWPORT_PICKER_H
