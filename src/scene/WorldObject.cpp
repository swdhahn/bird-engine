//
// Created by Seth Hahn on 11/30/22.
//

#include "WorldObject.h"

namespace bird {

    WorldObject::WorldObject()
    : m_worldPosition(), m_worldRotation(glm::quat(1.0, 0.0, 0.0, 0.0)), m_localPosition(), m_localRotation(glm::quat(1.0, 0.0, 0.0, 0.0)), m_scale(1),
    m_transformMatrix(1), m_needsMatrixUpdate(true), m_id(++s_uniqueIdentificationIndex)
    {}

    const uint64_t& WorldObject::getID() const {
        return m_id;
    }

    bool WorldObject::operator==(const WorldObject& e) {
        return e.getID() == m_id;
    }

    void WorldObject::translate(const Vector3& delta) {
        m_worldPosition += delta;
        m_needsMatrixUpdate = true;
    }

    void WorldObject::translateLocal(const Vector3& delta) {
        m_localPosition += delta;
        m_needsMatrixUpdate = true;
    }

    void WorldObject::rotate(const Vector3& axis, const float& angle) {
        m_worldRotation = fromAngleAxis(angle, axis) * m_worldRotation;
        m_needsMatrixUpdate = true;
    }

    void WorldObject::rotateLocal(const Vector3& axis, const float& angle) {
        m_localRotation = fromAngleAxis(angle, axis) * m_localRotation;
        m_needsMatrixUpdate = true;
    }

    const Vector3 &WorldObject::getWorldPosition() const {
        return m_worldPosition;
    }

    void WorldObject::setWorldPosition(Vector3 mWorldPosition) {
        m_worldPosition = mWorldPosition;
        m_needsMatrixUpdate = true;
    }

    const Vector3 &WorldObject::getLocalPosition() const {
        return m_localPosition;
    }

    void WorldObject::setLocalPosition(Vector3 mLocalPosition) {
        m_localPosition = mLocalPosition;
        m_needsMatrixUpdate = true;
    }

    const Quaternion &WorldObject::getWorldRotation() const {
        return m_worldRotation;
    }

    void WorldObject::setWorldRotation(Quaternion mWorldRotation) {
        m_worldRotation = mWorldRotation;
        m_needsMatrixUpdate = true;
    }

    const Quaternion &WorldObject::getLocalRotation() const {
        return m_localRotation;
    }

    void WorldObject::setLocalRotation(Quaternion mLocalRotation) {
        m_localRotation = mLocalRotation;
        m_needsMatrixUpdate = true;
    }

    const Matrix4 &WorldObject::getTransformMatrix() const {
        return m_transformMatrix;
    }

    void WorldObject::setTransformMatrix(Matrix4 transformMatrix) {
        m_transformMatrix = transformMatrix;
        m_needsMatrixUpdate = true;
    }

    void WorldObject::updateTransformationMatrix() {
        updateTransformMatrix(this, &m_transformMatrix);
    }

    bool WorldObject::needsMatrixUpdate() const {
        return m_needsMatrixUpdate;
    }

    const Vector3 &WorldObject::getScale() const {
        return m_scale;
    }

    void WorldObject::setScale(Vector3 scale) {
        m_scale = scale;
    }

} // bird