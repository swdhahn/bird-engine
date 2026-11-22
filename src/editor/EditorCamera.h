#ifndef BIRD_EDITOR_CAMERA_H
#define BIRD_EDITOR_CAMERA_H

#include "../scene/Camera.h"
#include "../util/Math.h"

namespace bird {
class Input;
class UIManager;
}  // namespace bird

namespace bird::editor {

/**
 * Sampled per-frame control state consumed by EditorCamera::update().
 *
 * Separating input sampling from camera integration keeps EditorCamera
 * deterministic and fully unit-testable without a live window.
 */
struct EditorCameraInput {
	/** Forward/backward movement axis in [-1, 1] (+1 = W, -1 = S). */
	float forward = 0.0f;
	/** Right/left strafe axis in [-1, 1] (+1 = D, -1 = A). */
	float right = 0.0f;
	/** World-vertical movement axis in [-1, 1] (+1 = E/Space, -1 = Q/Ctrl). */
	float up = 0.0f;
	/** True when Shift is held to multiply movement speed. */
	bool boost = false;
	/** True while right mouse button is held to rotate the camera. */
	bool lookActive = false;
	/** Mouse cursor delta in pixels (+x right, +y down). */
	Vector2 mouseDelta{0.0f};
	/** Mouse wheel vertical scroll delta (+y speeds up, -y slows down). */
	float scrollDelta = 0.0f;
};

/**
 * Viewport free-fly camera controller for Level Editor Edit Mode.
 *
 * Supports:
 * - Hold Right Mouse Button (RMB) to look around (yaw about world +Y, pitch
 *   about local +X clamped to [-89 deg, +89 deg]).
 * - WASD movement along the camera's forward/right basis, plus Q/E for
 *   vertical descent/ascent, and Shift for a speed boost.
 * - Mouse scroll wheel to multiplicatively adjust fly speed within
 *   [kMinSpeed, kMaxSpeed].
 * - Focus framing via focusOn() (bound to F in the editor).
 */
class EditorCamera {
   public:
	static constexpr float kDefaultSpeed = 10.0f;
	static constexpr float kMinSpeed = 0.5f;
	static constexpr float kMaxSpeed = 200.0f;
	static constexpr float kBoostMultiplier = 2.5f;
	static constexpr float kDefaultSensitivity = 0.0028f;  // radians / pixel
	static constexpr float kScrollSpeedFactor = 1.15f;

	/**
	 * Constructs an EditorCamera controller targeting @p camera.
	 * @param camera Non-owning pointer to the Camera entity to drive (may be null).
	 */
	explicit EditorCamera(Camera* camera = nullptr);

	/** Binds a Camera entity and synchronizes yaw/pitch from its orientation. */
	void setCamera(Camera* camera);

	/** Returns the currently controlled Camera entity (may be null). */
	Camera* getCamera() const { return m_camera; }

	/**
	 * Extracts yaw and pitch from the bound camera's current world rotation so
	 * taking control produces no sudden jump.
	 */
	void syncAnglesFromCamera();

	/**
	 * Advances the camera by @p delta seconds using explicit input values.
	 * @param input Sampled movement, look, and scroll state.
	 * @param delta Elapsed frame time in seconds.
	 */
	void update(const EditorCameraInput& input, float delta);

	/**
	 * Samples bird::Input (respecting UIManager focus/hover), manages mouse
	 * capture while RMB is held, and updates the camera.
	 * @param input Global input subsystem.
	 * @param ui Optional UIManager used to avoid stealing UI clicks/scroll/keys.
	 * @param delta Elapsed frame time in seconds.
	 * @return True while the user is actively flying (RMB held).
	 */
	bool updateFromInput(Input& input, const UIManager* ui, float delta);

	/**
	 * Positions the camera at @p distance units back from @p target along its
	 * current view direction so @p target is centered in the viewport.
	 * @param target World-space focal point.
	 * @param distance Desired distance from @p target (clamped >= 0.5).
	 */
	void focusOn(const Vector3& target, float distance = 5.0f);

	/** Sets base movement speed in world units per second (clamped to valid range). */
	void setMoveSpeed(float speed);
	/** Returns current base movement speed in world units per second. */
	float getMoveSpeed() const { return m_moveSpeed; }

	/** Sets mouse look sensitivity in radians per pixel. */
	void setSensitivity(float sensitivity) { m_sensitivity = sensitivity; }
	float getSensitivity() const { return m_sensitivity; }

	float getYaw() const { return m_yaw; }
	float getPitch() const { return m_pitch; }

	/** Sets yaw and pitch directly (in radians) and updates the camera rotation. */
	void setAngles(float yawRadians, float pitchRadians);

	/** @return True if right-mouse free-fly look was active on the last update. */
	bool isFlying() const { return m_flying; }

   private:
	void applyRotation();

	Camera* m_camera = nullptr;
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	float m_moveSpeed = kDefaultSpeed;
	float m_sensitivity = kDefaultSensitivity;
	bool m_flying = false;
};

}  // namespace bird::editor

#endif	// BIRD_EDITOR_CAMERA_H
