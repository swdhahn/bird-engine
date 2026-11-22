//
// Created by Seth Hahn on 11/30/22.
//

#include "WorldObject.h"

#include "../animation/AnimationSystem.h"
#include "../events/EventBus.h"
#include "../hotreload/EntityHandleRegistry.h"
#include "Scene.h"

namespace bird {

WorldObject::~WorldObject() {
	hotreload::EntityHandleRegistry::get().unregisterEntity(m_id);
	// Tweens hold raw pointers to their target; none may outlive it.
	if (ANIMATION_SYSTEM) ANIMATION_SYSTEM->cancelOwner(this);
	// Event listeners owned by this entity must never fire after destruction.
	if (EVENT_BUS) EVENT_BUS->unsubscribeByOwner(m_id);
}

WorldObject::WorldObject()
	: m_id(++s_uniqueIdentificationIndex),
	  m_uuid(UUID::generate()),
	  m_name(),
	  m_worldPosition(),
	  m_localPosition(),
	  m_scale(1),
	  m_worldRotation(glm::quat(1.0, 0.0, 0.0, 0.0)),
	  m_localRotation(glm::quat(1.0, 0.0, 0.0, 0.0)),
	  m_transformMatrix(1),
	  m_parent(nullptr),
	  m_needsMatrixUpdate(true),
	  m_isInitiated(false) {
	hotreload::EntityHandleRegistry::get().registerEntity(m_id, this);
}

void WorldObject::init(Scene* parent) {
	m_parent = parent;
	for (int i = 0; i < m_components.size(); i++) {
		m_components[i]->_init();
		m_components[i]->init();
	}
	m_isInitiated = true;
}

void WorldObject::deinit() {
	if (!m_isInitiated) return;
	for (int i = 0; i < m_components.size(); i++) {
		m_components[i]->deinit();
		m_components[i]->_deinit();
	}
	if (EVENT_BUS) EVENT_BUS->unsubscribeByOwner(m_id);
	m_isInitiated = false;
}

bool WorldObject::isInitiated() const {
	return m_isInitiated;
}

const uint64_t& WorldObject::getID() const {
	return m_id;
}

bool WorldObject::operator==(const WorldObject& e) {
	return e.getID() == m_id;
}

const UUID& WorldObject::getUUID() const {
	return m_uuid;
}

void WorldObject::setUUID(const UUID& uuid) {
	m_uuid = uuid;
}

std::string WorldObject::getUUIDString() const {
	return m_uuid.toString();
}

void WorldObject::setUUIDString(const std::string& uuid) {
	m_uuid = UUID::fromString(uuid);
}

const std::string& WorldObject::getName() const {
	return m_name;
}

void WorldObject::setName(const std::string& name) {
	m_name = name;
}

void WorldObject::translate(const Vector3& delta) {
	m_worldPosition += delta;
	markTransformChanged();
}

void WorldObject::translateLocal(const Vector3& delta) {
	m_localPosition += delta;
	markTransformChanged();
}

void WorldObject::rotate(const Vector3& axis, const float& angle) {
	m_worldRotation = fromAngleAxis(angle, axis) * m_worldRotation;
	markTransformChanged();
}

void WorldObject::rotateLocal(const Vector3& axis, const float& angle) {
	m_localRotation = fromAngleAxis(angle, axis) * m_localRotation;
	markTransformChanged();
}

const Vector3& WorldObject::getWorldPosition() const {
	return m_worldPosition;
}

void WorldObject::setWorldPosition(Vector3 mWorldPosition) {
	m_worldPosition = mWorldPosition;
	markTransformChanged();
}

const Vector3& WorldObject::getLocalPosition() const {
	return m_localPosition;
}

void WorldObject::setLocalPosition(Vector3 mLocalPosition) {
	m_localPosition = mLocalPosition;
	markTransformChanged();
}

const Quaternion& WorldObject::getWorldRotation() const {
	return m_worldRotation;
}

void WorldObject::setWorldRotation(Quaternion mWorldRotation) {
	m_worldRotation = mWorldRotation;
	markTransformChanged();
}

const Quaternion& WorldObject::getLocalRotation() const {
	return m_localRotation;
}

void WorldObject::setLocalRotation(Quaternion mLocalRotation) {
	m_localRotation = mLocalRotation;
	markTransformChanged();
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
	m_needsMatrixUpdate = false;
}

bool WorldObject::needsMatrixUpdate() const {
	return m_needsMatrixUpdate;
}

const Vector3& WorldObject::getScale() const {
	return m_scale;
}

void WorldObject::setScale(Vector3 scale) {
	m_scale = scale;
	markTransformChanged();
}

void WorldObject::markTransformChanged() {
	m_needsMatrixUpdate = true;
	m_transformRevision++;
}

uint64_t WorldObject::getTransformRevision() const {
	return m_transformRevision;
}

std::vector<_own<Component>>& WorldObject::getComponents() {
	return m_components;
}

void WorldObject::addComponent(_own<Component> component) {
	Component* c = component.get();
	m_components.emplace_back(_mv(component));
	c->setWorldObject(this);
	if (m_isInitiated) {
		c->_init();
		c->init();
	}
}

void WorldObject::removeComponent(Component* component) {
	if (m_isInitiated) {
		component->deinit();
		component->_deinit();
	}
	m_components.erase(std::remove_if(m_components.begin(), m_components.end(),
									  [&](_own<Component>& comp) {
										  return (component == comp.get());
									  }),
					   m_components.end());
}

Scene* WorldObject::getParent() {
	return m_parent;
}

}  // namespace bird
