//
// Created by Seth Hahn on 10/5/23.
//

#include "Camera.h"

#include "../Application.h"

namespace bird {

Camera::Camera(uint32_t width, uint32_t height) : WorldObject() {
	m_perspectiveMatrix =
		createPerspectiveMatrix(width, height, 70.0f, 0.1f, 1000.0f);
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

void Camera::setWorldEnvironment(WorldEnvironment env) {
	m_env = env;
}

WorldEnvironment& Camera::getWorldEnvironment() {
	return m_env;
}

}  // namespace bird
