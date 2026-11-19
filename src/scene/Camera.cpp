//
// Created by Seth Hahn on 10/5/23.
//

#include "Camera.h"

#include <stdexcept>

#include "../Application.h"

namespace bird {

Camera::Camera(uint32_t width, uint32_t height) : WorldObject() {
	setViewportSize(width, height);
	translate({0.0f, 0.0f, -5.0f});
	updateTransformationMatrix();
}

Camera::~Camera() {}

void Camera::enable() {
	APPLICATION->setCamera(this);
}

const Matrix4& Camera::getPerspectiveMatrix() const {
	return m_perspectiveMatrix;
}

void Camera::setViewportSize(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		throw std::invalid_argument("Camera: viewport size must be non-zero");
	}
	m_perspectiveMatrix = createPerspectiveMatrix(width, height, kFieldOfView,
												  kNearPlane, kFarPlane);
}

void Camera::setWorldEnvironment(WorldEnvironment env) {
	m_env = env;
}

WorldEnvironment& Camera::getWorldEnvironment() {
	return m_env;
}

}  // namespace bird
