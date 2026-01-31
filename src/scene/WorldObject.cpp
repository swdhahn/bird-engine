//
// Created by Seth Hahn on 11/30/22.
//

#include "WorldObject.h"

#include "components/PhysicsBodyComponent.h"

namespace bird {

WorldObject::WorldObject()
	: m_worldPosition(),
	  m_worldRotation(glm::quat(1.0, 0.0, 0.0, 0.0)),
	  m_localPosition(),
	  m_localRotation(glm::quat(1.0, 0.0, 0.0, 0.0)),
	  m_scale(1),
	  m_transformMatrix(1),
	  m_needsMatrixUpdate(true),
	  m_components(),
	  m_physicsBody(nullptr),
	  m_id(++s_uniqueIdentificationIndex) {}

WorldObject::~WorldObject() {}

void WorldObject::_deinit() {
	deinit();
	for (int i = 0; i < m_components.size(); i++) {
		m_components[i]->deinit();
	}
}

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

const Vector3& WorldObject::getWorldPosition() const {
	return m_worldPosition;
}

void WorldObject::setWorldPosition(Vector3 mWorldPosition) {
	m_worldPosition = mWorldPosition;
	m_needsMatrixUpdate = true;
}

const Vector3& WorldObject::getLocalPosition() const {
	return m_localPosition;
}

void WorldObject::setLocalPosition(Vector3 mLocalPosition) {
	m_localPosition = mLocalPosition;
	m_needsMatrixUpdate = true;
}

const Quaternion& WorldObject::getWorldRotation() const {
	return m_worldRotation;
}

void WorldObject::setWorldRotation(Quaternion mWorldRotation) {
	m_worldRotation = mWorldRotation;
	m_needsMatrixUpdate = true;
}

const Quaternion& WorldObject::getLocalRotation() const {
	return m_localRotation;
}

void WorldObject::setLocalRotation(Quaternion mLocalRotation) {
	m_localRotation = mLocalRotation;
	m_needsMatrixUpdate = true;
}

const Matrix4& WorldObject::getTransformMatrix() const {
	return m_transformMatrix;
}

void WorldObject::setTransformMatrix(Matrix4 transformMatrix) {
	m_transformMatrix = transformMatrix;
	m_needsMatrixUpdate = true;
}

void WorldObject::updateTransformationMatrix() {
	updateTransformMatrix(this, &m_transformMatrix);
	if (m_physicsBody != nullptr && !m_physicsBody->m_physicsUpdateTransfer)
		m_physicsBody->updateTransform();
}

bool WorldObject::needsMatrixUpdate() const {
	return m_needsMatrixUpdate;
}

const Vector3& WorldObject::getScale() const {
	return m_scale;
}

void WorldObject::setScale(Vector3 scale) {
	m_scale = scale;
}

void WorldObject::addComponent(std::unique_ptr<Component> component) {
	Component* c = component.get();
	PhysicsBodyComponent* phys = dynamic_cast<PhysicsBodyComponent*>(c);

	if (phys != nullptr) {
		if (m_physicsBody != nullptr) {
			std::cout << "You are trying to add another Physics Component when one already exists"
					  << std::endl;
			std::exit(-1);
		}
		m_physicsBody = phys;
	}
	m_components.emplace_back(std::move(component));
	c->setParent(this);
	c->init();
}

void WorldObject::removeComponent(Component* component) {
	component->deinit();
	PhysicsBodyComponent* phys = dynamic_cast<PhysicsBodyComponent*>(component);

	if (phys != nullptr) {
		m_physicsBody = nullptr;
	}
	m_components.erase(
			std::remove_if(m_components.begin(), m_components.end(),
					[&](std::unique_ptr<Component>& comp) { return (component == comp.get()); }),
			m_components.begin());
}

}  // namespace bird
