#ifndef BIRD_AUDIO_AUDIOSOURCECOMPONENT_H
#define BIRD_AUDIO_AUDIOSOURCECOMPONENT_H

#include <string>

#include "../scene/components/Component.h"
#include "../util/Math.h"
#include "AudioTypes.h"

namespace bird {

class AudioSystem;

/**
 * Persistent audio playback component attached to a WorldObject.
 *
 * Supports both 3D spatialized sound sources (default) and 2D non-spatialized
 * sources, with disk streaming, bus routing, looping, fade-in/fade-out,
 * distance attenuation, directional cones, and automatic Doppler velocity
 * tracking synchronized every frame with the host WorldObject's transform.
 *
 * Lifecycle:
 * - Registers with AUDIO_SYSTEM in _init(). If playOnInit is true and a clip
 *   path is configured, playback starts automatically.
 * - Stops playback and unregisters cleanly in _deinit() and the destructor.
 */
class AudioSourceComponent : public Component {
   public:
	/** Constructs an empty 3D AudioSourceComponent routed to AudioBus::kSFX. */
	AudioSourceComponent();

	/**
	 * Constructs an AudioSourceComponent configured with an audio file path
	 * and target mixing bus.
	 *
	 * @param clipPath Path to the audio file (WAV, MP3, or FLAC).
	 * @param bus Target mixing bus name (defaults to AudioBus::kSFX).
	 */
	explicit AudioSourceComponent(const std::string& clipPath,
								  const std::string& bus = AudioBus::kSFX);

	~AudioSourceComponent() override;

	void init() override {}
	void deinit() override {}
	void _init() override;
	void _deinit() override;

	// ------------------------------------------------------- Playback Control

	/**
	 * Starts or restarts playback of the configured clip.
	 * If a sound is already playing on this source, it is stopped first.
	 */
	void play();

	/**
	 * Starts playback with a smooth linear fade-in over fadeInSeconds.
	 *
	 * @param fadeInSeconds Fade-in duration in seconds (<= 0 starts immediately).
	 */
	void playFadeIn(float fadeInSeconds);

	/** Pauses playback while retaining the current playback cursor. */
	void pause();

	/** Resumes playback from where it was paused. */
	void resume();

	/** Immediately stops playback and releases the underlying sound handle. */
	void stop();

	/**
	 * Fades playback volume to zero over fadeOutSeconds and stops when finished.
	 *
	 * @param fadeOutSeconds Fade-out duration in seconds (<= 0 stops immediately).
	 */
	void stopFadeOut(float fadeOutSeconds);

	/** Returns true if this source is currently playing audio. */
	bool isPlaying() const;

	/** Returns true if this source is currently paused. */
	bool isPaused() const;

	/** Returns the active AudioHandle (or a null handle if stopped). */
	AudioHandle getHandle() const { return m_handle; }

	// ---------------------------------------------------------- Configuration

	/** Sets the audio file path to play (takes effect on the next play() call). */
	void setClip(const std::string& clipPath) { m_clipPath = clipPath; }
	/** Returns the configured audio file path. */
	const std::string& getClip() const { return m_clipPath; }

	/** Sets the target mixing bus name (takes effect on the next play() call). */
	void setBus(const std::string& bus) { m_bus = bus; }
	/** Returns the configured target mixing bus name. */
	const std::string& getBus() const { return m_bus; }

	/** Sets the linear volume (>= 0.0); updates live if currently playing. */
	void setVolume(float volume);
	/** Returns the configured linear volume. */
	float getVolume() const { return m_volume; }

	/** Sets the pitch multiplier (>= 0.01); updates live if currently playing. */
	void setPitch(float pitch);
	/** Returns the configured pitch multiplier. */
	float getPitch() const { return m_pitch; }

	/** Sets stereo pan in [-1.0, 1.0] for 2D sources; updates live if playing. */
	void setPan(float pan);
	/** Returns the configured stereo pan. */
	float getPan() const { return m_pan; }

	/** Enables or disables looping; updates live if currently playing. */
	void setLooping(bool looping);
	/** Returns true if looping is enabled. */
	bool isLooping() const { return m_looping; }

	/**
	 * Enables or disables disk streaming (takes effect on next play()).
	 * Set to true for long ambient beds or music tracks to avoid full RAM decode.
	 */
	void setStreaming(bool streaming) { m_streaming = streaming; }
	/** Returns true if this source streams audio from disk. */
	bool isStreaming() const { return m_streaming; }

	/**
	 * Enables or disables 3D spatialization (takes effect on next play()).
	 * Default is true (3D positional audio).
	 */
	void setSpatialized(bool spatialized) { m_spatialized = spatialized; }
	/** Returns true if this source plays as a 3D spatialized sound. */
	bool isSpatialized() const { return m_spatialized; }

	/** If true, automatically starts playback when the host WorldObject initializes. */
	void setPlayOnInit(bool playOnInit) { m_playOnInit = playOnInit; }
	/** Returns true if playback starts automatically on component init. */
	bool getPlayOnInit() const { return m_playOnInit; }

	// ------------------------------------------------------------ 3D Settings

	/** Replaces all 3D spatialization settings; updates live if playing. */
	void setSpatialSettings(const Audio3DSettings& settings);
	/** Returns the current 3D spatialization settings. */
	const Audio3DSettings& getSpatialSettings() const { return m_spatial; }

	/** Sets the distance attenuation model; updates live if playing. */
	void setAttenuationModel(AudioAttenuationModel model);
	/** Returns the configured distance attenuation model. */
	AudioAttenuationModel getAttenuationModel() const { return m_spatial.attenuation; }

	/** Sets the minimum attenuation distance; updates live if playing. */
	void setMinDistance(float distance);
	/** Returns the minimum attenuation distance. */
	float getMinDistance() const { return m_spatial.minDistance; }

	/** Sets the maximum attenuation distance; updates live if playing. */
	void setMaxDistance(float distance);
	/** Returns the maximum attenuation distance. */
	float getMaxDistance() const { return m_spatial.maxDistance; }

	/** Sets the attenuation rolloff factor; updates live if playing. */
	void setRolloff(float rolloff);
	/** Returns the attenuation rolloff factor. */
	float getRolloff() const { return m_spatial.rolloff; }

	/** Sets the Doppler shift intensity (0 disables Doppler); updates live. */
	void setDopplerFactor(float factor);
	/** Returns the Doppler shift intensity. */
	float getDopplerFactor() const { return m_spatial.dopplerFactor; }

	/**
	 * Overrides automatic displacement-based velocity calculation with an
	 * explicit world-space velocity vector (units/sec) for Doppler shift.
	 */
	void setVelocityOverride(const Vector3& velocity);

	/** Re-enables automatic velocity calculation from WorldObject movement. */
	void clearVelocityOverride();

	// -------------------------------------------------------- Engine Internal

	/**
	 * Engine-internal hook called by AudioSystem::tick() every frame to push
	 * the host WorldObject's 3D position, direction, and velocity to the sound.
	 *
	 * @param delta Elapsed frame time in seconds.
	 */
	void _syncTransform(float delta);

	/** Engine-internal hook called if AudioSystem shuts down before this component. */
	void _forgetSystem();

   private:
	Vector3 computeWorldPosition() const;
	Vector3 computeWorldForward() const;

	AudioSystem* m_system = nullptr;
	AudioHandle m_handle{};

	std::string m_clipPath;
	std::string m_bus = AudioBus::kSFX;
	float m_volume = 1.0f;
	float m_pitch = 1.0f;
	float m_pan = 0.0f;
	bool m_looping = false;
	bool m_streaming = false;
	bool m_spatialized = true;
	bool m_playOnInit = false;

	Audio3DSettings m_spatial{};
	bool m_hasPrevPosition = false;
	Vector3 m_prevPosition{0.0f, 0.0f, 0.0f};
	bool m_hasVelocityOverride = false;
	Vector3 m_velocityOverride{0.0f, 0.0f, 0.0f};
};

}  // namespace bird

#endif	// BIRD_AUDIO_AUDIOSOURCECOMPONENT_H
