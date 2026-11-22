#include "EditorCamera.h"

#include <algorithm>
#include <cmath>

#include "../scene/2D/UIManager.h"
#include "../util/Input.h"

namespace bird::editor {

namespace {
constexpr float kMaxPitch = 89.0f * 3.14159265358979f / 180.0f;
}

EditorCamera::EditorCamera(Camera* camera) : m_camera(camera) {
	if (m_camera) {
		syncAnglesFromCamera();
	}
}

void EditorCamera::setCamera(Camera* camera) {
	m_camera = camera;
	if (m_camera) {
		syncAnglesFromCamera();
	}
}

void EditorCamera::syncAnglesFromCamera() {
	if (!m_camera) return;
	// Camera looks down local -Z; derive yaw/pitch from world forward vector.
	const Vector3 forward =
		glm::normalize(m_camera->getWorldRotation() * Vector3(0.0f, 0.0f, -1.0f));
	m_pitch = std::clamp(std::asin(std::clamp(forward.y, -1.0f, 1.0f)),
						 -kMaxPitch, kMaxPitch);
	m_yaw = std::atan2(-forward.x, -forward.z);
}

void EditorCamera::setAngles(float yawRadians, float pitchRadians) {
	m_yaw = yawRadians;
	m_pitch = std::clamp(pitchRadians, -kMaxPitch, kMaxPitch);
	applyRotation();
}

void EditorCamera::setMoveSpeed(float speed) {
	m_moveSpeed = std::clamp(speed, kMinSpeed, kMaxSpeed);
}

void EditorCamera::applyRotation() {
	if (!m_camera) return;
	const Quaternion qYaw = glm::angleAxis(m_yaw, Vector3(0.0f, 1.0f, 0.0f));
	const Quaternion qPitch = glm::angleAxis(m_pitch, Vector3(1.0f, 0.0f, 0.0f));
	m_camera->setWorldRotation(qYaw * qPitch);
}

void EditorCamera::update(const EditorCameraInput& input, float delta) {
	if (!m_camera) return;

	if (input.scrollDelta != 0.0f) {
		const float factor = std::pow(kScrollSpeedFactor, input.scrollDelta);
		setMoveSpeed(m_moveSpeed * factor);
	}

	m_flying = input.lookActive;
	if (input.lookActive) {
		m_yaw -= input.mouseDelta.x * m_sensitivity;
		m_pitch = std::clamp(m_pitch - input.mouseDelta.y * m_sensitivity,
							 -kMaxPitch, kMaxPitch);
		applyRotation();
	}

	if (delta <= 0.0f) return;

	const bool hasMove =
		(input.forward != 0.0f || input.right != 0.0f || input.up != 0.0f);
	if (!hasMove) return;

	const Quaternion rot = m_camera->getWorldRotation();
	const Vector3 forwardDir = rot * Vector3(0.0f, 0.0f, -1.0f);
	const Vector3 rightDir = rot * Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 upDir(0.0f, 1.0f, 0.0f);

	Vector3 move =
		forwardDir * input.forward + rightDir * input.right + upDir * input.up;
	const float len = glm::length(move);
	if (len > 1e-5f) {
		move /= len;
	}

	const float speed = m_moveSpeed * (input.boost ? kBoostMultiplier : 1.0f);
	m_camera->translate(move * speed * delta);
}

bool EditorCamera::updateFromInput(Input& input, const UIManager* ui,
								   float delta) {
	if (!m_camera) return false;

	const bool uiWantsMouse = ui && ui->wantsMouse();
	const bool uiWantsKeyboard = ui && ui->wantsKeyboard();

	const bool rmbDown = input.isMouseButtonDown(MOUSE_BUTTON_RIGHT);
	// Begin flying only when RMB is pressed over the 3D viewport (not UI),
	// and keep flying until RMB is released.
	const bool lookActive = m_flying ? rmbDown : (rmbDown && !uiWantsMouse);
	input.setMouseCaptured(lookActive);

	EditorCameraInput camInput;
	camInput.lookActive = lookActive;
	camInput.mouseDelta = lookActive ? input.getMouseDelta() : Vector2(0.0f);

	// Allow scroll wheel speed adjustment while flying or hovering the viewport.
	if (lookActive || !uiWantsMouse) {
		camInput.scrollDelta = input.getScrollDelta().y;
	}

	// WASD + Q/E movement is active while flying (or when UI doesn't want keys
	// and RMB is held, avoiding conflicts with W/E/R gizmo hotkeys when not
	// flying).
	if (lookActive && !uiWantsKeyboard) {
		const float fwd = (input.isKeyDown(KEY_W) ? 1.0f : 0.0f) -
						  (input.isKeyDown(KEY_S) ? 1.0f : 0.0f);
		const float rgt = (input.isKeyDown(KEY_D) ? 1.0f : 0.0f) -
						  (input.isKeyDown(KEY_A) ? 1.0f : 0.0f);
		const float up =
			((input.isKeyDown(KEY_E) || input.isKeyDown(KEY_SPACE)) ? 1.0f
																	: 0.0f) -
			((input.isKeyDown(KEY_Q) || input.isKeyDown(KEY_LEFT_CONTROL))
				 ? 1.0f
				 : 0.0f);
		camInput.forward = fwd;
		camInput.right = rgt;
		camInput.up = up;
		camInput.boost = input.isKeyDown(KEY_LEFT_SHIFT) ||
						 input.isKeyDown(KEY_RIGHT_SHIFT);
	}

	update(camInput, delta);
	return m_flying;
}

void EditorCamera::focusOn(const Vector3& target, float distance) {
	if (!m_camera) return;
	const float clampedDist = std::max(distance, 0.5f);
	const Vector3 forward =
		glm::normalize(m_camera->getWorldRotation() * Vector3(0.0f, 0.0f, -1.0f));
	m_camera->setWorldPosition(target - forward * clampedDist);
}

}  // namespace bird::editor
