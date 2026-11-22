#include "AudioListenerComponent.h"

#include "../scene/WorldObject.h"
#include "AudioSystem.h"

namespace bird {

AudioListenerComponent::AudioListenerComponent(bool active)
	: m_active(active) {}

AudioListenerComponent::~AudioListenerComponent() {
	if (m_system) {
		m_system->_unregisterListener(this);
		m_system = nullptr;
	}
}

void AudioListenerComponent::_init() {
	m_system = AUDIO_SYSTEM;
	m_hasPrevPosition = false;
	if (m_system) {
		m_system->_registerListener(this);
		if (m_active) {
			if (m_hasCone) {
				m_system->setListenerCone(m_coneInner, m_coneOuter,
										  m_coneOuterGain);
			}
			_syncTransform(0.0f);
		}
	}
}

void AudioListenerComponent::_deinit() {
	if (m_system) {
		m_system->_unregisterListener(this);
		m_system = nullptr;
	}
	m_hasPrevPosition = false;
}

void AudioListenerComponent::setActive(bool active) {
	m_active = active;
	if (!m_system) return;
	if (active) {
		m_system->_setActiveListener(this);
		if (m_hasCone) {
			m_system->setListenerCone(m_coneInner, m_coneOuter,
									  m_coneOuterGain);
		}
		_syncTransform(0.0f);
	} else {
		m_system->_setActiveListener(nullptr);
	}
}

void AudioListenerComponent::setVelocityOverride(const Vector3& velocity) {
	m_hasVelocityOverride = true;
	m_velocityOverride = velocity;
}

void AudioListenerComponent::clearVelocityOverride() {
	m_hasVelocityOverride = false;
}

void AudioListenerComponent::setCone(float innerAngleRad, float outerAngleRad,
									 float outerGain) {
	m_hasCone = true;
	m_coneInner = innerAngleRad;
	m_coneOuter = outerAngleRad;
	m_coneOuterGain = outerGain;
	if (m_system && m_active) {
		m_system->setListenerCone(innerAngleRad, outerAngleRad, outerGain);
	}
}

void AudioListenerComponent::_syncTransform(float delta) {
	if (!m_system || !m_active || !m_object) return;

	const Vector3 pos =
		m_object->getWorldPosition() + m_object->getLocalPosition();
	const Quaternion rot =
		m_object->getWorldRotation() * m_object->getLocalRotation();
	const Vector3 forward = glm::normalize(rot * Vector3(0.0f, 0.0f, -1.0f));
	const Vector3 up = glm::normalize(rot * Vector3(0.0f, 1.0f, 0.0f));

	Vector3 velocity(0.0f, 0.0f, 0.0f);
	if (m_hasVelocityOverride) {
		velocity = m_velocityOverride;
	} else if (m_hasPrevPosition && delta > 1e-6f) {
		velocity = (pos - m_prevPosition) / delta;
	}

	m_prevPosition = pos;
	m_hasPrevPosition = true;

	m_system->setListenerTransform(pos, forward, up, velocity);
}

}  // namespace bird
