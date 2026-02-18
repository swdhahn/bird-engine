#include "ScreenObject.h"

namespace bird {

ScreenObject::ScreenObject()
	: m_position(),
	  m_rotation(0),
	  m_needsMatrixUpdate(false),
	  m_transformMatrix(1),
	  m_scale(1) {}

ScreenObject::~ScreenObject() {}

const Vector2& ScreenObject::getPosition() const {
	return m_position;
}
void ScreenObject::setPosition(Vector2 position) {
	m_position = position;
	m_needsMatrixUpdate = true;
}
const float& ScreenObject::getRotation() const {
	return m_rotation;
}
void ScreenObject::setRotation(float rotation) {
	m_rotation = rotation;
	m_needsMatrixUpdate = true;
}
const Matrix3& ScreenObject::getTransformMatrix() const {
	return m_transformMatrix;
}
const Vector2& ScreenObject::getScale() const {
	return m_scale;
}
void ScreenObject::setScale(Vector2 scale) {
	m_scale = scale;
	m_needsMatrixUpdate = true;
}

void ScreenObject::setTransformMatrix(Matrix3 transformMatrix) {
	m_transformMatrix = transformMatrix;
	m_needsMatrixUpdate = true;
}

void ScreenObject::updateTransformationMatrix() {
	updateTransformMatrix(this, &m_transformMatrix);
}
bool ScreenObject::needsMatrixUpdate() const {
	return m_needsMatrixUpdate;
}

}  // namespace bird
