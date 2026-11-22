#ifndef BIRD_AUDIO_AUDIOTYPES_H
#define BIRD_AUDIO_AUDIOTYPES_H

#include <cstdint>
#include <string>

#include "../util/Constants.h"
#include "../util/Math.h"

namespace bird {

/**
 * Well-known names of the built-in hierarchical mixing buses.
 *
 * Hierarchy:
 *   Master
 *     ├── Music
 *     ├── SFX
 *     └── Voice
 *
 * Custom buses can be added at runtime via AudioSystem::createBus().
 */
struct AudioBus {
	/** Root mixing bus; all other buses route into Master. */
	static constexpr const char* kMaster = "Master";
	/** Bus intended for background music and streamed soundtracks. */
	static constexpr const char* kMusic = "Music";
	/** Bus intended for gameplay and UI sound effects. */
	static constexpr const char* kSFX = "SFX";
	/** Bus intended for dialogue, narration, and character voices. */
	static constexpr const char* kVoice = "Voice";
};

/**
 * Generational handle referencing an active sound instance in AudioSystem.
 *
 * Handles never dangle: once a sound finishes or is stopped its slot
 * generation increments, and subsequent operations on stale handles safely
 * no-op or return default values.
 */
struct AudioHandle {
	/** Slot index inside AudioSystem (UINT32_MAX denotes a null handle). */
	uint32_t index = UINT32_MAX;
	/** Generation counter guarding against ABA reuse of retired slots. */
	uint32_t generation = 0;

	/** Returns true if this handle was default-constructed and references no sound. */
	bool isNull() const { return index == UINT32_MAX; }
};

/**
 * Distance attenuation curve used for 3D spatialized sound sources.
 */
enum class AudioAttenuationModel : int32_t {
	/** No distance attenuation; sound stays at full gain regardless of distance. */
	None = 0,
	/** Physically inspired inverse-distance rolloff (default). */
	Inverse = 1,
	/** Linear falloff from full gain at minDistance to minGain at maxDistance. */
	Linear = 2,
	/** Exponential distance rolloff controlled by the rolloff exponent. */
	Exponential = 3
};

/**
 * Initialization settings for AudioSystem.
 */
struct AudioSystemConfig {
	/** Output sample rate in Hz (0 selects the platform device's native rate, or 48000 in no-device mode). */
	uint32_t sampleRate = 0;
	/** Number of output channels (default 2 for stereo). */
	uint32_t channels = 2;
	/**
	 * When true, initializes the audio graph without opening a hardware
	 * playback device. Useful for headless servers, CI, and offline PCM tests.
	 */
	bool noDevice = false;
};

/**
 * Playback parameters shared by 2D and 3D sounds.
 */
struct AudioPlayOptions {
	/** Target mixing bus name (defaults to AudioBus::kSFX). */
	std::string bus = AudioBus::kSFX;
	/** Linear gain multiplier (1.0 = unity, 0.0 = silent; must be >= 0). */
	float volume = 1.0f;
	/** Playback pitch / speed multiplier (1.0 = normal; must be > 0). */
	float pitch = 1.0f;
	/** Stereo pan for 2D sounds in [-1.0 (left), 1.0 (right)], 0.0 = center. */
	float pan = 0.0f;
	/** When true, the sound loops seamlessly until stopped. */
	bool looping = false;
	/**
	 * When true, streams compressed or uncompressed audio incrementally from
	 * disk instead of decoding the entire asset into RAM. Recommended for
	 * long music tracks and ambient beds.
	 */
	bool stream = false;
	/** Fade-in duration in seconds applied when playback starts (0 = instant). */
	float fadeInSeconds = 0.0f;
	/** When true, initializes the sound in a paused state without starting it. */
	bool startPaused = false;
};

/**
 * Spatialization, distance attenuation, directional cone, and Doppler
 * parameters for a 3D sound source.
 */
struct Audio3DSettings {
	/** Distance attenuation curve. */
	AudioAttenuationModel attenuation = AudioAttenuationModel::Inverse;
	/** Distance at which the sound begins to attenuate (meters / world units). */
	float minDistance = 1.0f;
	/** Maximum distance used by attenuation models (meters / world units). */
	float maxDistance = 100.0f;
	/** Attenuation rolloff steepness factor (1.0 = standard). */
	float rolloff = 1.0f;
	/** Minimum gain floor after distance attenuation (in [0, 1]). */
	float minGain = 0.0f;
	/** Maximum gain ceiling after distance attenuation (in [0, 1]). */
	float maxGain = 1.0f;
	/** Doppler shift intensity (0.0 disables Doppler, 1.0 = physical default). */
	float dopplerFactor = 1.0f;
	/** Inner directional cone angle in radians (full gain inside; 2*pi = omnidirectional). */
	float coneInnerAngle = 6.2831853f;
	/** Outer directional cone angle in radians (transitions to coneOuterGain outside). */
	float coneOuterAngle = 6.2831853f;
	/** Gain multiplier outside the outer directional cone (in [0, 1]). */
	float coneOuterGain = 0.0f;
};

}  // namespace bird

#endif	// BIRD_AUDIO_AUDIOTYPES_H
