#ifndef BIRD_EDITOR_TRANSFORM_GIZMO_H
#define BIRD_EDITOR_TRANSFORM_GIZMO_H

#include <cstdint>
#include <memory>

#include "../renders/DebugDraw.h"
#include "../scene/Camera.h"
#include "../scene/WorldObject.h"
#include "../util/Math.h"
#include "UndoStack.h"
#include "ViewportPicker.h"

namespace bird::editor {

/** Active manipulation mode of the 3D TransformGizmo. */
enum class GizmoMode : uint8_t {
	Translate = 0,
	Rotate = 1,
	Scale = 2
};

/** Coordinate frame used by the 3D TransformGizmo. */
enum class GizmoSpace : uint8_t {
	World = 0,
	Local = 1
};

/** Specific gizmo handle under the cursor or being dragged. */
enum class GizmoAxis : uint8_t {
	None = 0,
	X,
	Y,
	Z,
	XY,
	XZ,
	YZ,
	Uniform
};

/**
 * Interactive 3D Translate / Rotate / Scale gizmo rendered via DebugDrawList.
 *
 * Scales dynamically with camera distance to maintain a constant apparent
 * screen size. Supports both World and Local coordinate spaces, axis and plane
 * hit-testing against picking rays, live entity mutation during dragging, and
 * emits a single TransformCommand on drag completion for UndoStack integration.
 */
class TransformGizmo {
   public:
	/** Apparent size multiplier relative to camera distance. */
	static constexpr float kScreenSizeFactor = 0.16f;
	/** Minimum world-space gizmo radius when the camera is very close. */
	static constexpr float kMinWorldScale = 0.25f;

	TransformGizmo() = default;

	/** Sets the manipulation mode (Translate, Rotate, or Scale). */
	void setMode(GizmoMode mode);
	GizmoMode getMode() const { return m_mode; }

	/** Sets the coordinate frame (World or Local). */
	void setSpace(GizmoSpace space);
	GizmoSpace getSpace() const { return m_space; }

	/** Returns the handle currently hovered by the cursor (or None). */
	GizmoAxis getHoveredAxis() const { return m_hoveredAxis; }
	/** Returns the handle currently being dragged (or None). */
	GizmoAxis getActiveAxis() const { return m_activeAxis; }
	/** @return True while a gizmo drag is in progress. */
	bool isDragging() const { return m_activeAxis != GizmoAxis::None; }

	/**
	 * Computes the world-space scale of the gizmo at @p origin so it maintains
	 * a consistent apparent size on screen.
	 */
	float computeScale(const Vector3& origin, const Camera& camera) const;

	/**
	 * Computes the three orthonormal gizmo axes for @p target according to the
	 * current GizmoSpace (World or Local).
	 */
	void getAxes(const WorldObject& target, Vector3& axisX, Vector3& axisY,
				 Vector3& axisZ) const;

	/**
	 * Tests @p ray against all handles of the current GizmoMode.
	 * @param ray World-space cursor ray.
	 * @param target Entity at which the gizmo is anchored.
	 * @param camera Active viewport camera (used for screen-space sizing).
	 * @return Closest hit GizmoAxis, or GizmoAxis::None.
	 */
	GizmoAxis hitTest(const Ray& ray, const WorldObject& target,
					  const Camera& camera) const;

	/**
	 * Updates hovered-axis highlighting without starting a drag.
	 */
	void updateHover(const Ray& ray, const WorldObject* target,
					 const Camera& camera);

	/**
	 * Attempts to start dragging a gizmo handle on @p target.
	 * @param ray World-space cursor ray at mouse-down.
	 * @param target Selected entity to manipulate.
	 * @param camera Active viewport camera.
	 * @return True if a handle was hit and dragging began; false otherwise.
	 */
	bool beginDrag(const Ray& ray, WorldObject* target, const Camera& camera);

	/**
	 * Continues an active drag, mutating the target entity's transform live.
	 * @param ray Updated world-space cursor ray.
	 * @param camera Active viewport camera.
	 */
	void updateDrag(const Ray& ray, const Camera& camera);

	/**
	 * Completes an active drag and produces a TransformCommand if the entity
	 * actually moved/rotated/scaled.
	 * @return Owned TransformCommand ready for UndoStack::push(), or nullptr.
	 */
	std::unique_ptr<TransformCommand> endDrag();

	/**
	 * Aborts an active drag and restores the entity's pre-drag transform.
	 */
	void cancelDrag();

	/**
	 * Emits always-on-top overlay debug lines for the gizmo at @p target.
	 * @param debugDraw Destination DebugDrawList (uses overlayLine()).
	 * @param target Selected entity (no-op if null).
	 * @param camera Active viewport camera.
	 */
	void draw(DebugDrawList& debugDraw, const WorldObject* target,
			  const Camera& camera) const;

   private:
	Vector3 entityOrigin(const WorldObject& target) const;
	Vector3 axisDirection(GizmoAxis axis) const;
	Vector3 planeNormal(GizmoAxis planeAxis) const;

	static bool intersectRayPlane(const Ray& ray, const Vector3& planeOrigin,
								  const Vector3& planeNormal,
								  Vector3& outHit);
	static float closestParamOnAxis(const Ray& ray, const Vector3& axisOrigin,
									const Vector3& axisDir);
	static float distanceRayToSegment(const Ray& ray, const Vector3& a,
									  const Vector3& b, float& outRayT);

	GizmoMode m_mode = GizmoMode::Translate;
	GizmoSpace m_space = GizmoSpace::World;
	GizmoAxis m_hoveredAxis = GizmoAxis::None;
	GizmoAxis m_activeAxis = GizmoAxis::None;

	WorldObject* m_dragTarget = nullptr;
	TransformSnapshot m_beforeSnapshot;
	Vector3 m_dragOrigin{0.0f};
	Vector3 m_dragAxisX{1.0f, 0.0f, 0.0f};
	Vector3 m_dragAxisY{0.0f, 1.0f, 0.0f};
	Vector3 m_dragAxisZ{0.0f, 0.0f, 1.0f};
	float m_dragScale = 1.0f;
	float m_dragStartParam = 0.0f;
	Vector3 m_dragStartPlaneHit{0.0f};
	Vector3 m_dragStartRotVec{1.0f, 0.0f, 0.0f};
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_TRANSFORM_GIZMO_H
