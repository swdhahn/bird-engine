#include "AudioInternal.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "../platform/Log.h"
#include "../util/ResourceManager.h"
#include "AudioListenerComponent.h"
#include "AudioSourceComponent.h"

namespace bird {

namespace {

ma_attenuation_model toMaAttenuation(AudioAttenuationModel model) {
	switch (model) {
		case AudioAttenuationModel::None:
			return ma_attenuation_model_none;
		case AudioAttenuationModel::Linear:
			return ma_attenuation_model_linear;
		case AudioAttenuationModel::Exponential:
			return ma_attenuation_model_exponential;
		case AudioAttenuationModel::Inverse:
		default:
			return ma_attenuation_model_inverse;
	}
}

void applySpatialSettings(ma_sound* sound, const Audio3DSettings& spatial) {
	ma_sound_set_attenuation_model(sound, toMaAttenuation(spatial.attenuation));
	ma_sound_set_min_distance(sound, std::max(0.0f, spatial.minDistance));
	ma_sound_set_max_distance(sound,
							  std::max(spatial.minDistance, spatial.maxDistance));
	ma_sound_set_rolloff(sound, std::max(0.0f, spatial.rolloff));
	ma_sound_set_min_gain(sound, std::clamp(spatial.minGain, 0.0f, 1.0f));
	ma_sound_set_max_gain(sound, std::clamp(spatial.maxGain, 0.0f, 1.0f));
	ma_sound_set_doppler_factor(sound, std::max(0.0f, spatial.dopplerFactor));
	ma_sound_set_cone(sound, spatial.coneInnerAngle, spatial.coneOuterAngle,
					  std::clamp(spatial.coneOuterGain, 0.0f, 1.0f));
}

}  // namespace

AudioSystem::AudioSystem() : m_impl(_own_mk<Impl>()) {}

AudioSystem::~AudioSystem() {
	deinit();
}

bool AudioSystem::init(const AudioSystemConfig& config) {
	if (m_impl->initialized) return true;

	ma_engine_config engineConfig = ma_engine_config_init();
	engineConfig.channels = config.channels > 0 ? config.channels : 2;
	engineConfig.sampleRate = config.sampleRate;
	engineConfig.listenerCount = 1;

	bool useNoDevice = config.noDevice;
	if (useNoDevice) {
		engineConfig.noDevice = MA_TRUE;
		if (engineConfig.sampleRate == 0) {
			engineConfig.sampleRate = 48000;
		}
	}

	ma_result result = ma_engine_init(&engineConfig, &m_impl->engine);
	if (result != MA_SUCCESS && !useNoDevice) {
		platform::log(
			platform::LogLevel::Warning,
			"AudioSystem: hardware audio device unavailable (miniaudio error " +
				std::to_string(static_cast<int>(result)) +
				"); falling back to no-device mode.");
		engineConfig = ma_engine_config_init();
		engineConfig.channels = config.channels > 0 ? config.channels : 2;
		engineConfig.sampleRate =
			config.sampleRate > 0 ? config.sampleRate : 48000;
		engineConfig.listenerCount = 1;
		engineConfig.noDevice = MA_TRUE;
		useNoDevice = true;
		result = ma_engine_init(&engineConfig, &m_impl->engine);
	}

	if (result != MA_SUCCESS) {
		platform::log(platform::LogLevel::Error,
					  "AudioSystem: failed to initialize miniaudio engine (" +
						  std::to_string(static_cast<int>(result)) + ").");
		return false;
	}

	m_impl->initialized = true;
	m_impl->noDevice = useNoDevice;

	// Create root Master bus connected directly to the engine endpoint.
	auto masterNode = std::make_unique<AudioBusNode>();
	masterNode->name = AudioBus::kMaster;
	masterNode->parentName.clear();
	if (ma_sound_group_init(&m_impl->engine, 0, nullptr, &masterNode->group) !=
		MA_SUCCESS) {
		platform::log(platform::LogLevel::Error,
					  "AudioSystem: failed to create Master mixing bus.");
		deinit();
		return false;
	}
	masterNode->initialized = true;
	m_impl->buses.emplace(AudioBus::kMaster, std::move(masterNode));
	m_impl->busOrder.push_back(AudioBus::kMaster);

	// Create default child buses under Master.
	createBus(AudioBus::kMusic, AudioBus::kMaster);
	createBus(AudioBus::kSFX, AudioBus::kMaster);
	createBus(AudioBus::kVoice, AudioBus::kMaster);

	// Default listener at origin looking down -Z with +Y up (right-handed).
	setListenerTransform(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, -1.0f),
						 Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f));

	return true;
}

void AudioSystem::deinit() {
	if (!m_impl) return;

	// Detach any remaining scene components so their destructors stay safe.
	for (AudioSourceComponent* src : m_impl->sources) {
		if (src) src->_forgetSystem();
	}
	m_impl->sources.clear();

	for (AudioListenerComponent* listener : m_impl->listeners) {
		if (listener) listener->_forgetSystem();
	}
	m_impl->listeners.clear();
	m_impl->activeListener = nullptr;

	if (!m_impl->initialized) return;

	// Tear down all active sound instances first.
	for (uint32_t i = 0; i < m_impl->slots.size(); ++i) {
		if (m_impl->slots[i].active) {
			releaseSlot(i);
		}
	}
	m_impl->slots.clear();
	m_impl->freeSlots.clear();
	m_impl->activeCount = 0;

	// Uninitialize buses in reverse creation order (children before parents).
	for (auto it = m_impl->busOrder.rbegin(); it != m_impl->busOrder.rend();
		 ++it) {
		auto busIt = m_impl->buses.find(*it);
		if (busIt != m_impl->buses.end() && busIt->second->initialized) {
			ma_sound_group_uninit(&busIt->second->group);
			busIt->second->initialized = false;
		}
	}
	m_impl->buses.clear();
	m_impl->busOrder.clear();

	ma_engine_uninit(&m_impl->engine);
	m_impl->initialized = false;
	m_impl->noDevice = false;
}

bool AudioSystem::isInitialized() const {
	return m_impl && m_impl->initialized;
}

bool AudioSystem::isNoDeviceMode() const {
	return m_impl && m_impl->noDevice;
}

uint32_t AudioSystem::getSampleRate() const {
	if (!isInitialized()) return 0;
	return ma_engine_get_sample_rate(&m_impl->engine);
}

uint32_t AudioSystem::getChannels() const {
	if (!isInitialized()) return 0;
	return ma_engine_get_channels(&m_impl->engine);
}

void AudioSystem::tick(float delta) {
	if (!isInitialized()) return;

	// Retire finished one-shots and completed fade-outs.
	for (uint32_t i = 0; i < m_impl->slots.size(); ++i) {
		SoundSlot& slot = m_impl->slots[i];
		if (!slot.active || !slot.sound) continue;
		if (slot.paused) continue;

		const bool playing = ma_sound_is_playing(slot.sound.get()) == MA_TRUE;
		const bool atEnd = ma_sound_at_end(slot.sound.get()) == MA_TRUE;
		if (!playing && (slot.oneShot || slot.stoppingWithFade ||
						 (!slot.looping && atEnd))) {
			releaseSlot(i);
		}
	}

	// Synchronize active 3D listener transform and velocity.
	if (m_impl->activeListener) {
		m_impl->activeListener->_syncTransform(delta);
	}

	// Synchronize all registered 3D audio sources.
	for (AudioSourceComponent* source : m_impl->sources) {
		if (source) {
			source->_syncTransform(delta);
		}
	}
}

// ---------------------------------------------------------------- Mixing Buses

bool AudioSystem::createBus(const std::string& name,
							const std::string& parentName) {
	if (!isInitialized() || name.empty()) return false;
	if (m_impl->buses.find(name) != m_impl->buses.end()) return false;

	auto parentIt = m_impl->buses.find(parentName);
	if (parentIt == m_impl->buses.end()) return false;

	auto node = std::make_unique<AudioBusNode>();
	node->name = name;
	node->parentName = parentName;

	ma_result res = ma_sound_group_init(&m_impl->engine, 0,
										&parentIt->second->group, &node->group);
	if (res != MA_SUCCESS) {
		platform::log(platform::LogLevel::Error,
					  "AudioSystem: failed to create bus '" + name + "'.");
		return false;
	}
	node->initialized = true;
	m_impl->buses.emplace(name, std::move(node));
	m_impl->busOrder.push_back(name);
	return true;
}

bool AudioSystem::hasBus(const std::string& name) const {
	if (!m_impl) return false;
	return m_impl->buses.find(name) != m_impl->buses.end();
}

std::string AudioSystem::getBusParent(const std::string& name) const {
	if (!m_impl) return "";
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return "";
	return it->second->parentName;
}

std::vector<std::string> AudioSystem::getBusNames() const {
	if (!m_impl) return {};
	return m_impl->busOrder;
}

void AudioSystem::setBusVolume(const std::string& name, float volume) {
	if (!isInitialized()) return;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return;

	AudioBusNode* node = it->second.get();
	node->volume = std::max(0.0f, volume);
	ma_sound_group_set_volume(&node->group, node->muted ? 0.0f : node->volume);
}

float AudioSystem::getBusVolume(const std::string& name) const {
	if (!m_impl) return 1.0f;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return 1.0f;
	return it->second->volume;
}

void AudioSystem::setBusMuted(const std::string& name, bool muted) {
	if (!isInitialized()) return;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return;

	AudioBusNode* node = it->second.get();
	node->muted = muted;
	ma_sound_group_set_volume(&node->group, node->muted ? 0.0f : node->volume);
}

bool AudioSystem::isBusMuted(const std::string& name) const {
	if (!m_impl) return false;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return false;
	return it->second->muted;
}

void AudioSystem::setBusPaused(const std::string& name, bool paused) {
	if (!isInitialized()) return;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return;

	AudioBusNode* node = it->second.get();
	if (node->paused == paused) return;
	node->paused = paused;
	if (paused) {
		ma_sound_group_stop(&node->group);
	} else {
		ma_sound_group_start(&node->group);
	}
}

bool AudioSystem::isBusPaused(const std::string& name) const {
	if (!m_impl) return false;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return false;
	return it->second->paused;
}

void AudioSystem::setBusPitch(const std::string& name, float pitch) {
	if (!isInitialized()) return;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return;

	it->second->pitch = std::max(0.01f, pitch);
	refreshSoundPitches();
}

float AudioSystem::getBusPitch(const std::string& name) const {
	if (!m_impl) return 1.0f;
	auto it = m_impl->buses.find(name);
	if (it == m_impl->buses.end()) return 1.0f;
	return it->second->pitch;
}

void AudioSystem::setMasterVolume(float volume) {
	setBusVolume(AudioBus::kMaster, volume);
}

float AudioSystem::getMasterVolume() const {
	return getBusVolume(AudioBus::kMaster);
}

void AudioSystem::setMasterMuted(bool muted) {
	setBusMuted(AudioBus::kMaster, muted);
}

bool AudioSystem::isMasterMuted() const {
	return isBusMuted(AudioBus::kMaster);
}

// ------------------------------------------------------------------- Playback

AudioHandle AudioSystem::playOneShot(const std::string& path,
									 const std::string& bus, float volume,
									 float pitch) {
	AudioPlayOptions opts;
	opts.bus = bus;
	opts.volume = volume;
	opts.pitch = pitch;
	opts.looping = false;
	opts.stream = false;
	return playInternal(path, false, Vector3(0.0f), Audio3DSettings{}, opts,
						/*oneShot=*/true);
}

AudioHandle AudioSystem::play2D(const std::string& path,
								const AudioPlayOptions& options) {
	return playInternal(path, false, Vector3(0.0f), Audio3DSettings{}, options,
						/*oneShot=*/false);
}

AudioHandle AudioSystem::playMusic(const std::string& path, bool loop,
								   float fadeInSeconds, float volume) {
	AudioPlayOptions opts;
	opts.bus = AudioBus::kMusic;
	opts.volume = volume;
	opts.looping = loop;
	opts.stream = true;
	opts.fadeInSeconds = fadeInSeconds;
	return playInternal(path, false, Vector3(0.0f), Audio3DSettings{}, opts,
						/*oneShot=*/false);
}

AudioHandle AudioSystem::playOneShot3D(const std::string& path,
									   const Vector3& position,
									   const std::string& bus, float volume,
									   float pitch) {
	AudioPlayOptions opts;
	opts.bus = bus;
	opts.volume = volume;
	opts.pitch = pitch;
	opts.looping = false;
	opts.stream = false;
	return playInternal(path, true, position, Audio3DSettings{}, opts,
						/*oneShot=*/true);
}

AudioHandle AudioSystem::play3D(const std::string& path,
								const Vector3& position,
								const Audio3DSettings& spatial,
								const AudioPlayOptions& options) {
	return playInternal(path, true, position, spatial, options,
						/*oneShot=*/false);
}

AudioHandle AudioSystem::playInternal(const std::string& path, bool spatialized,
									  const Vector3& position,
									  const Audio3DSettings& spatial,
									  const AudioPlayOptions& options,
									  bool oneShot) {
	if (!isInitialized() || path.empty()) return AudioHandle{};

	const std::string resolvedPath = resolveAudioPath(path);
	std::string targetBus = options.bus.empty() ? AudioBus::kSFX : options.bus;
	auto busIt = m_impl->buses.find(targetBus);
	if (busIt == m_impl->buses.end()) {
		platform::log(platform::LogLevel::Warning,
					  "AudioSystem: unknown bus '" + targetBus +
						  "'; routing sound to Master.");
		targetBus = AudioBus::kMaster;
		busIt = m_impl->buses.find(targetBus);
		if (busIt == m_impl->buses.end()) return AudioHandle{};
	}

	ma_uint32 flags =
		options.stream ? MA_SOUND_FLAG_STREAM : MA_SOUND_FLAG_DECODE;
	if (!spatialized) {
		flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
	}

	auto sound = std::make_unique<ma_sound>();
	ma_result res = ma_sound_init_from_file(
		&m_impl->engine, resolvedPath.c_str(), flags, &busIt->second->group,
		nullptr, sound.get());
	if (res != MA_SUCCESS) {
		platform::log(platform::LogLevel::Warning,
					  "AudioSystem: failed to load audio file '" +
						  resolvedPath + "' (miniaudio error " +
						  std::to_string(static_cast<int>(res)) + ").");
		return AudioHandle{};
	}

	const float clampedVolume = std::max(0.0f, options.volume);
	const float clampedPitch = std::max(0.01f, options.pitch);
	const float effectivePitch =
		clampedPitch * computeEffectiveBusPitch(targetBus);

	ma_sound_set_volume(sound.get(), clampedVolume);
	ma_sound_set_pitch(sound.get(), effectivePitch);
	ma_sound_set_looping(sound.get(), options.looping ? MA_TRUE : MA_FALSE);

	if (spatialized) {
		ma_sound_set_spatialization_enabled(sound.get(), MA_TRUE);
		ma_sound_set_position(sound.get(), position.x, position.y, position.z);
		applySpatialSettings(sound.get(), spatial);
	} else {
		ma_sound_set_pan(sound.get(), std::clamp(options.pan, -1.0f, 1.0f));
	}

	if (!options.startPaused) {
		if (options.fadeInSeconds > 0.0f) {
			const ma_uint64 fadeMs = static_cast<ma_uint64>(
				std::max(1.0f, options.fadeInSeconds * 1000.0f));
			ma_sound_set_fade_in_milliseconds(sound.get(), 0.0f, clampedVolume,
											  fadeMs);
		}
		ma_sound_start(sound.get());
	}

	uint32_t index = 0;
	if (!m_impl->freeSlots.empty()) {
		index = m_impl->freeSlots.back();
		m_impl->freeSlots.pop_back();
	} else {
		index = static_cast<uint32_t>(m_impl->slots.size());
		m_impl->slots.emplace_back();
	}

	SoundSlot& slot = m_impl->slots[index];
	slot.sound = std::move(sound);
	slot.busName = targetBus;
	slot.volume = clampedVolume;
	slot.pitch = clampedPitch;
	slot.looping = options.looping;
	slot.spatialized = spatialized;
	slot.streaming = options.stream;
	slot.paused = options.startPaused;
	slot.oneShot = oneShot;
	slot.stoppingWithFade = false;
	slot.active = true;
	m_impl->activeCount++;

	return AudioHandle{index, slot.generation};
}

// ------------------------------------------------------- Sound Handle Control

bool AudioSystem::isValid(AudioHandle handle) const {
	return m_impl->resolveSlot(handle) != nullptr;
}

bool AudioSystem::isPlaying(AudioHandle handle) const {
	const SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound || slot->paused) return false;
	return ma_sound_is_playing(slot->sound.get()) == MA_TRUE;
}

bool AudioSystem::isPaused(AudioHandle handle) const {
	const SoundSlot* slot = m_impl->resolveSlot(handle);
	return slot ? slot->paused : false;
}

void AudioSystem::pause(AudioHandle handle) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound || slot->paused) return;
	ma_sound_stop(slot->sound.get());
	slot->paused = true;
}

void AudioSystem::resume(AudioHandle handle) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound || !slot->paused) return;
	slot->paused = false;
	ma_sound_start(slot->sound.get());
}

void AudioSystem::stop(AudioHandle handle) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot) return;
	releaseSlot(handle.index);
}

void AudioSystem::stopWithFade(AudioHandle handle, float fadeOutSeconds) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	if (fadeOutSeconds <= 0.0f) {
		releaseSlot(handle.index);
		return;
	}
	const ma_uint64 fadeMs =
		static_cast<ma_uint64>(std::max(1.0f, fadeOutSeconds * 1000.0f));
	const ma_uint64 stopTimeMs =
		ma_engine_get_time_in_milliseconds(&m_impl->engine) + fadeMs;
	ma_sound_set_stop_time_with_fade_in_milliseconds(slot->sound.get(),
													 stopTimeMs, fadeMs);
	slot->stoppingWithFade = true;
	slot->paused = false;
}

void AudioSystem::fadeIn(AudioHandle handle, float fadeInSeconds) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	if (fadeInSeconds > 0.0f) {
		const ma_uint64 fadeMs =
			static_cast<ma_uint64>(std::max(1.0f, fadeInSeconds * 1000.0f));
		ma_sound_set_fade_in_milliseconds(slot->sound.get(), 0.0f, slot->volume,
										  fadeMs);
	} else {
		ma_sound_set_volume(slot->sound.get(), slot->volume);
	}
	if (slot->paused) {
		slot->paused = false;
		ma_sound_start(slot->sound.get());
	}
}

void AudioSystem::fadeTo(AudioHandle handle, float targetVolume,
						 float durationSeconds) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	const float clamped = std::max(0.0f, targetVolume);
	slot->volume = clamped;
	if (durationSeconds > 0.0f) {
		const ma_uint64 fadeMs =
			static_cast<ma_uint64>(std::max(1.0f, durationSeconds * 1000.0f));
		// Passing -1.0f as volumeBeg tells miniaudio to start from the current volume.
		ma_sound_set_fade_in_milliseconds(slot->sound.get(), -1.0f, clamped,
										  fadeMs);
	} else {
		ma_sound_set_volume(slot->sound.get(), clamped);
	}
}

void AudioSystem::setVolume(AudioHandle handle, float volume) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	slot->volume = std::max(0.0f, volume);
	ma_sound_set_volume(slot->sound.get(), slot->volume);
}

float AudioSystem::getVolume(AudioHandle handle) const {
	const SoundSlot* slot = m_impl->resolveSlot(handle);
	return slot ? slot->volume : 0.0f;
}

void AudioSystem::setPitch(AudioHandle handle, float pitch) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	slot->pitch = std::max(0.01f, pitch);
	ma_sound_set_pitch(slot->sound.get(),
					   slot->pitch * computeEffectiveBusPitch(slot->busName));
}

float AudioSystem::getPitch(AudioHandle handle) const {
	const SoundSlot* slot = m_impl->resolveSlot(handle);
	return slot ? slot->pitch : 1.0f;
}

void AudioSystem::setPan(AudioHandle handle, float pan) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	ma_sound_set_pan(slot->sound.get(), std::clamp(pan, -1.0f, 1.0f));
}

void AudioSystem::setLooping(AudioHandle handle, bool looping) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	slot->looping = looping;
	ma_sound_set_looping(slot->sound.get(), looping ? MA_TRUE : MA_FALSE);
}

bool AudioSystem::isLooping(AudioHandle handle) const {
	const SoundSlot* slot = m_impl->resolveSlot(handle);
	return slot ? slot->looping : false;
}

void AudioSystem::setPosition(AudioHandle handle, const Vector3& position) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	ma_sound_set_position(slot->sound.get(), position.x, position.y,
						  position.z);
}

void AudioSystem::setVelocity(AudioHandle handle, const Vector3& velocity) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	ma_sound_set_velocity(slot->sound.get(), velocity.x, velocity.y,
						  velocity.z);
}

void AudioSystem::setDirection(AudioHandle handle, const Vector3& direction) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	ma_sound_set_direction(slot->sound.get(), direction.x, direction.y,
						   direction.z);
}

void AudioSystem::setSpatialSettings(AudioHandle handle,
									 const Audio3DSettings& settings) {
	SoundSlot* slot = m_impl->resolveSlot(handle);
	if (!slot || !slot->sound) return;
	applySpatialSettings(slot->sound.get(), settings);
}

void AudioSystem::stopAll() {
	if (!isInitialized()) return;
	for (uint32_t i = 0; i < m_impl->slots.size(); ++i) {
		if (m_impl->slots[i].active) {
			releaseSlot(i);
		}
	}
}

size_t AudioSystem::getActiveSoundCount() const {
	return m_impl ? m_impl->activeCount : 0;
}

// ----------------------------------------------------------- Listener Control

void AudioSystem::setListenerTransform(const Vector3& position,
									   const Vector3& forward,
									   const Vector3& up,
									   const Vector3& velocity) {
	if (!isInitialized()) return;
	m_impl->listenerPos = position;
	m_impl->listenerForward = forward;
	m_impl->listenerUp = up;
	m_impl->listenerVelocity = velocity;

	ma_engine_listener_set_position(&m_impl->engine, 0, position.x, position.y,
									position.z);
	ma_engine_listener_set_direction(&m_impl->engine, 0, forward.x, forward.y,
									 forward.z);
	ma_engine_listener_set_world_up(&m_impl->engine, 0, up.x, up.y, up.z);
	ma_engine_listener_set_velocity(&m_impl->engine, 0, velocity.x, velocity.y,
									velocity.z);
}

Vector3 AudioSystem::getListenerPosition() const {
	return m_impl ? m_impl->listenerPos : Vector3(0.0f);
}

Vector3 AudioSystem::getListenerForward() const {
	return m_impl ? m_impl->listenerForward : Vector3(0.0f, 0.0f, -1.0f);
}

Vector3 AudioSystem::getListenerUp() const {
	return m_impl ? m_impl->listenerUp : Vector3(0.0f, 1.0f, 0.0f);
}

Vector3 AudioSystem::getListenerVelocity() const {
	return m_impl ? m_impl->listenerVelocity : Vector3(0.0f);
}

void AudioSystem::setListenerCone(float innerAngleRad, float outerAngleRad,
								  float outerGain) {
	if (!isInitialized()) return;
	ma_engine_listener_set_cone(&m_impl->engine, 0, innerAngleRad,
								outerAngleRad,
								std::clamp(outerGain, 0.0f, 1.0f));
}

// -------------------------------------------------- Offline & Engine Internal

uint64_t AudioSystem::readPcmFrames(float* outFrames, uint64_t frameCount) {
	if (!isInitialized() || !outFrames || frameCount == 0) return 0;
	ma_uint64 framesRead = 0;
	ma_result res = ma_engine_read_pcm_frames(&m_impl->engine, outFrames,
											  frameCount, &framesRead);
	if (res != MA_SUCCESS) return 0;
	return static_cast<uint64_t>(framesRead);
}

void AudioSystem::_registerSource(AudioSourceComponent* source) {
	if (!m_impl || !source) return;
	auto& list = m_impl->sources;
	if (std::find(list.begin(), list.end(), source) == list.end()) {
		list.push_back(source);
	}
}

void AudioSystem::_unregisterSource(AudioSourceComponent* source) {
	if (!m_impl || !source) return;
	auto& list = m_impl->sources;
	list.erase(std::remove(list.begin(), list.end(), source), list.end());
}

void AudioSystem::_registerListener(AudioListenerComponent* listener) {
	if (!m_impl || !listener) return;
	auto& list = m_impl->listeners;
	if (std::find(list.begin(), list.end(), listener) == list.end()) {
		list.push_back(listener);
	}
	if (listener->isActive()) {
		_setActiveListener(listener);
	}
}

void AudioSystem::_unregisterListener(AudioListenerComponent* listener) {
	if (!m_impl || !listener) return;
	auto& list = m_impl->listeners;
	list.erase(std::remove(list.begin(), list.end(), listener), list.end());
	if (m_impl->activeListener == listener) {
		m_impl->activeListener = nullptr;
		// Promote another registered listener if any remain.
		for (AudioListenerComponent* candidate : list) {
			if (candidate && candidate->isActive()) {
				m_impl->activeListener = candidate;
				break;
			}
		}
	}
}

void AudioSystem::_setActiveListener(AudioListenerComponent* listener) {
	if (!m_impl) return;
	if (m_impl->activeListener && m_impl->activeListener != listener) {
		m_impl->activeListener->_setActiveFlag(false);
	}
	m_impl->activeListener = listener;
	if (listener) {
		listener->_setActiveFlag(true);
	}
}

// -------------------------------------------------------------------- Helpers

float AudioSystem::computeEffectiveBusPitch(const std::string& busName) const {
	float pitch = 1.0f;
	std::string current = busName;
	// Guard against any hypothetical cycles (max depth 64).
	for (int depth = 0; depth < 64 && !current.empty(); ++depth) {
		auto it = m_impl->buses.find(current);
		if (it == m_impl->buses.end()) break;
		pitch *= it->second->pitch;
		current = it->second->parentName;
	}
	return std::max(0.01f, pitch);
}

void AudioSystem::refreshSoundPitches() {
	for (SoundSlot& slot : m_impl->slots) {
		if (!slot.active || !slot.sound) continue;
		const float effective =
			slot.pitch * computeEffectiveBusPitch(slot.busName);
		ma_sound_set_pitch(slot.sound.get(), effective);
	}
}

void AudioSystem::releaseSlot(uint32_t index) {
	if (index >= m_impl->slots.size()) return;
	SoundSlot& slot = m_impl->slots[index];
	if (!slot.active) return;

	if (slot.sound) {
		ma_sound_stop(slot.sound.get());
		ma_sound_uninit(slot.sound.get());
		slot.sound.reset();
	}
	slot.active = false;
	slot.paused = false;
	slot.stoppingWithFade = false;
	slot.generation++;
	m_impl->freeSlots.push_back(index);
	if (m_impl->activeCount > 0) {
		m_impl->activeCount--;
	}
}

std::string AudioSystem::resolveAudioPath(const std::string& path) const {
	namespace fs = std::filesystem;
	std::error_code ec;
	if (fs::exists(path, ec)) {
		return path;
	}
	if (!ResourceManager::RESOURCE_PATH.empty()) {
		const std::string candidate = ResourceManager::RESOURCE_PATH + path;
		if (fs::exists(candidate, ec)) {
			return candidate;
		}
	}
	return path;
}

}  // namespace bird
