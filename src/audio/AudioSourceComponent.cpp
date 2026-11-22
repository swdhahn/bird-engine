#include "AudioSourceComponent.h"

#include "../scene/WorldObject.h"
#include "AudioSystem.h"

namespace bird {

AudioSourceComponent::AudioSourceComponent() = default;

AudioSourceComponent::AudioSourceComponent(const std::string& clipPath,
										   const std::string& bus)
	: m_clipPath(clipPath), m_bus(bus) {}

AudioSourceComponent::~AudioSourceComponent() {
	stop();
	if (m_system) {
		m_system->_unregisterSource(this);
		m_system = nullptr;
	}
}

void AudioSourceComponent::_init() {
	m_system = AUDIO_SYSTEM;
	m_hasPrevPosition = false;
	if (m_system) {
		m_system->_registerSource(this);
		if (m_playOnInit && !m_clipPath.empty()) {
			play();
		}
	}
}

void AudioSourceComponent::_deinit() {
	stop();
	if (m_system) {
		m_system->_unregisterSource(this);
		m_system = nullptr;
	}
	m_hasPrevPosition = false;
}

void AudioSourceComponent::play() {
	playFadeIn(0.0f);
}

void AudioSourceComponent::playFadeIn(float fadeInSeconds) {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (!sys || !sys->isInitialized() || m_clipPath.empty()) return;

	if (!m_handle.isNull()) {
		sys->stop(m_handle);
		m_handle = AudioHandle{};
	}

	AudioPlayOptions opts;
	opts.bus = m_bus;
	opts.volume = m_volume;
	opts.pitch = m_pitch;
	opts.pan = m_pan;
	opts.looping = m_looping;
	opts.stream = m_streaming;
	opts.fadeInSeconds = fadeInSeconds;

	if (m_spatialized) {
		const Vector3 pos = computeWorldPosition();
		m_handle = sys->play3D(m_clipPath, pos, m_spatial, opts);
		if (!m_handle.isNull()) {
			sys->setDirection(m_handle, computeWorldForward());
			if (m_hasVelocityOverride) {
				sys->setVelocity(m_handle, m_velocityOverride);
			}
			m_prevPosition = pos;
			m_hasPrevPosition = true;
		}
	} else {
		m_handle = sys->play2D(m_clipPath, opts);
	}
}

void AudioSourceComponent::pause() {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->pause(m_handle);
	}
}

void AudioSourceComponent::resume() {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->resume(m_handle);
	}
}

void AudioSourceComponent::stop() {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->stop(m_handle);
	}
	m_handle = AudioHandle{};
}

void AudioSourceComponent::stopFadeOut(float fadeOutSeconds) {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (!sys || m_handle.isNull()) return;
	if (fadeOutSeconds <= 0.0f) {
		stop();
	} else {
		sys->stopWithFade(m_handle, fadeOutSeconds);
	}
}

bool AudioSourceComponent::isPlaying() const {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	return sys && !m_handle.isNull() && sys->isPlaying(m_handle);
}

bool AudioSourceComponent::isPaused() const {
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	return sys && !m_handle.isNull() && sys->isPaused(m_handle);
}

void AudioSourceComponent::setVolume(float volume) {
	m_volume = std::max(0.0f, volume);
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->setVolume(m_handle, m_volume);
	}
}

void AudioSourceComponent::setPitch(float pitch) {
	m_pitch = std::max(0.01f, pitch);
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->setPitch(m_handle, m_pitch);
	}
}

void AudioSourceComponent::setPan(float pan) {
	m_pan = std::clamp(pan, -1.0f, 1.0f);
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull() && !m_spatialized) {
		sys->setPan(m_handle, m_pan);
	}
}

void AudioSourceComponent::setLooping(bool looping) {
	m_looping = looping;
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull()) {
		sys->setLooping(m_handle, m_looping);
	}
}

void AudioSourceComponent::setSpatialSettings(const Audio3DSettings& settings) {
	m_spatial = settings;
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull() && m_spatialized) {
		sys->setSpatialSettings(m_handle, m_spatial);
	}
}

void AudioSourceComponent::setAttenuationModel(AudioAttenuationModel model) {
	m_spatial.attenuation = model;
	setSpatialSettings(m_spatial);
}

void AudioSourceComponent::setMinDistance(float distance) {
	m_spatial.minDistance = std::max(0.0f, distance);
	setSpatialSettings(m_spatial);
}

void AudioSourceComponent::setMaxDistance(float distance) {
	m_spatial.maxDistance = std::max(m_spatial.minDistance, distance);
	setSpatialSettings(m_spatial);
}

void AudioSourceComponent::setRolloff(float rolloff) {
	m_spatial.rolloff = std::max(0.0f, rolloff);
	setSpatialSettings(m_spatial);
}

void AudioSourceComponent::setDopplerFactor(float factor) {
	m_spatial.dopplerFactor = std::max(0.0f, factor);
	setSpatialSettings(m_spatial);
}

void AudioSourceComponent::setVelocityOverride(const Vector3& velocity) {
	m_hasVelocityOverride = true;
	m_velocityOverride = velocity;
	AudioSystem* sys = m_system ? m_system : AUDIO_SYSTEM;
	if (sys && !m_handle.isNull() && m_spatialized) {
		sys->setVelocity(m_handle, m_velocityOverride);
	}
}

void AudioSourceComponent::clearVelocityOverride() {
	m_hasVelocityOverride = false;
}

void AudioSourceComponent::_syncTransform(float delta) {
	if (!m_system || !m_spatialized || m_handle.isNull()) return;
	if (!m_system->isValid(m_handle)) {
		m_handle = AudioHandle{};
		return;
	}
	if (!m_object) return;

	const Vector3 pos = computeWorldPosition();
	const Vector3 forward = computeWorldForward();

	Vector3 velocity(0.0f, 0.0f, 0.0f);
	if (m_hasVelocityOverride) {
		velocity = m_velocityOverride;
	} else if (m_hasPrevPosition && delta > 1e-6f) {
		velocity = (pos - m_prevPosition) / delta;
	}

	m_prevPosition = pos;
	m_hasPrevPosition = true;

	m_system->setPosition(m_handle, pos);
	m_system->setDirection(m_handle, forward);
	m_system->setVelocity(m_handle, velocity);
}

void AudioSourceComponent::_forgetSystem() {
	m_system = nullptr;
	m_handle = AudioHandle{};
}

Vector3 AudioSourceComponent::computeWorldPosition() const {
	if (!m_object) return Vector3(0.0f);
	return m_object->getWorldPosition() + m_object->getLocalPosition();
}

Vector3 AudioSourceComponent::computeWorldForward() const {
	if (!m_object) return Vector3(0.0f, 0.0f, -1.0f);
	const Quaternion rot =
		m_object->getWorldRotation() * m_object->getLocalRotation();
	return glm::normalize(rot * Vector3(0.0f, 0.0f, -1.0f));
}

}  // namespace bird
