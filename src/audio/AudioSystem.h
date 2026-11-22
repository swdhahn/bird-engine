#ifndef BIRD_AUDIO_AUDIOSYSTEM_H
#define BIRD_AUDIO_AUDIOSYSTEM_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"
#include "../util/Memory.hpp"
#include "AudioTypes.h"

namespace bird {

class AudioSourceComponent;
class AudioListenerComponent;

/**
 * Engine-wide audio subsystem built on a hierarchical node-graph mixer.
 *
 * Encapsulates all low-level audio backend state so the rest of the engine
 * never includes backend headers directly. Provides:
 * - Hierarchical mixing buses (Master -> Music, SFX, Voice, plus custom buses)
 *   with per-bus volume, mute, pause/resume, and pitch control.
 * - 2D audio playback for UI clicks, ambient beds, and disk-streamed music.
 * - 3D spatialized playback with distance attenuation, directional cones,
 *   Doppler velocity, and automatic per-frame synchronization with
 *   AudioListenerComponent and AudioSourceComponent instances.
 * - Both fire-and-forget one-shots and handle-controlled persistent sounds
 *   with sample-accurate fade-in and fade-out.
 *
 * Lifecycle: Application::start() creates and initializes the global
 * AUDIO_SYSTEM instance before loading the root Scene, ticks it once per frame
 * in Application::tick(), and destroys it after the Scene in Application::deinit().
 */
class AudioSystem {
   public:
	AudioSystem();
	~AudioSystem();
	AudioSystem(const AudioSystem&) = delete;
	AudioSystem& operator=(const AudioSystem&) = delete;

	// ------------------------------------------------------------ Lifecycle

	/**
	 * Initializes the audio backend and builds the default mixing bus hierarchy
	 * (Master -> Music, SFX, Voice).
	 *
	 * If opening a hardware playback device fails (for example on a headless
	 * Linux host without an audio daemon), automatically falls back to
	 * no-device mode so the engine continues running cleanly.
	 *
	 * @param config Optional sample rate, channel count, and no-device settings.
	 * @return true if initialization succeeded (including fallback to no-device).
	 */
	bool init(const AudioSystemConfig& config = {});

	/**
	 * Stops all active sounds, tears down all mixing buses, and shuts down
	 * the audio backend. Safe to call multiple times.
	 */
	void deinit();

	/** Returns true when the audio engine has been initialized. */
	bool isInitialized() const;

	/** Returns true when running without a hardware playback device. */
	bool isNoDeviceMode() const;

	/** Returns the active output sample rate in Hz (0 if uninitialized). */
	uint32_t getSampleRate() const;

	/** Returns the active output channel count (0 if uninitialized). */
	uint32_t getChannels() const;

	/**
	 * Advances per-frame audio housekeeping: retires finished one-shot sounds
	 * and synchronizes 3D transforms and velocities for the active
	 * AudioListenerComponent and all registered AudioSourceComponent instances.
	 *
	 * Called once per frame by Application::tick() after scene transforms update.
	 *
	 * @param delta Elapsed frame time in seconds.
	 */
	void tick(float delta);

	// ---------------------------------------------------------- Mixing Buses

	/**
	 * Creates a custom mixing bus routed into an existing parent bus.
	 *
	 * @param name Unique name for the new bus (must be non-empty).
	 * @param parentName Name of the existing parent bus (defaults to AudioBus::kMaster).
	 * @return true if the bus was created; false if the name already exists,
	 *         is empty, the parent bus does not exist, or the system is uninitialized.
	 */
	bool createBus(const std::string& name,
				   const std::string& parentName = AudioBus::kMaster);

	/**
	 * Returns true if a mixing bus with the given name exists.
	 * @param name Bus name to query.
	 */
	bool hasBus(const std::string& name) const;

	/**
	 * Returns the parent bus name for the given bus, or an empty string for
	 * Master or an unknown bus.
	 * @param name Bus name to query.
	 */
	std::string getBusParent(const std::string& name) const;

	/** Returns the names of all registered mixing buses in creation order. */
	std::vector<std::string> getBusNames() const;

	/**
	 * Sets the linear volume of a mixing bus (clamped to >= 0.0).
	 * Affects all sounds routed directly to this bus and any child buses.
	 * Preserved independently of mute state.
	 *
	 * @param name Target bus name.
	 * @param volume Linear gain (1.0 = unity, 0.0 = silent).
	 */
	void setBusVolume(const std::string& name, float volume);

	/**
	 * Returns the configured linear volume of a mixing bus (1.0 if unknown).
	 * @param name Target bus name.
	 */
	float getBusVolume(const std::string& name) const;

	/**
	 * Mutes or unmutes a mixing bus without altering its stored volume.
	 * Affects all sounds routed through this bus and its descendants.
	 *
	 * @param name Target bus name.
	 * @param muted True to silence the bus, false to restore its volume.
	 */
	void setBusMuted(const std::string& name, bool muted);

	/**
	 * Returns true if the specified mixing bus is currently muted.
	 * @param name Target bus name.
	 */
	bool isBusMuted(const std::string& name) const;

	/**
	 * Pauses or resumes a mixing bus in the audio node graph.
	 * Pausing a bus freezes playback of all sounds and child buses routed
	 * through it; resuming continues seamlessly from where they paused.
	 *
	 * @param name Target bus name.
	 * @param paused True to pause, false to resume.
	 */
	void setBusPaused(const std::string& name, bool paused);

	/**
	 * Returns true if the specified mixing bus is currently paused.
	 * @param name Target bus name.
	 */
	bool isBusPaused(const std::string& name) const;

	/**
	 * Sets the pitch multiplier for a mixing bus (clamped to >= 0.01).
	 * Composes multiplicatively down the bus hierarchy (Master * child * sound).
	 *
	 * @param name Target bus name.
	 * @param pitch Pitch multiplier (1.0 = normal speed/pitch).
	 */
	void setBusPitch(const std::string& name, float pitch);

	/**
	 * Returns the configured pitch multiplier of a mixing bus (1.0 if unknown).
	 * @param name Target bus name.
	 */
	float getBusPitch(const std::string& name) const;

	/** Convenience shortcut for setBusVolume(AudioBus::kMaster, volume). */
	void setMasterVolume(float volume);
	/** Convenience shortcut for getBusVolume(AudioBus::kMaster). */
	float getMasterVolume() const;
	/** Convenience shortcut for setBusMuted(AudioBus::kMaster, muted). */
	void setMasterMuted(bool muted);
	/** Convenience shortcut for isBusMuted(AudioBus::kMaster). */
	bool isMasterMuted() const;

	// ----------------------------------------------------------- 2D Playback

	/**
	 * Plays a fire-and-forget 2D sound effect (UI clicks, impacts, etc.).
	 * The sound is decoded into memory for low-latency playback and
	 * automatically cleaned up once it finishes.
	 *
	 * @param path Path to the audio file (WAV, MP3, or FLAC). Relative paths
	 *             are resolved against ResourceManager::RESOURCE_PATH if needed.
	 * @param bus Target mixing bus (defaults to AudioBus::kSFX).
	 * @param volume Linear gain multiplier (default 1.0).
	 * @param pitch Pitch multiplier (default 1.0).
	 * @return Handle to the playing sound, or a null AudioHandle on failure.
	 */
	AudioHandle playOneShot(const std::string& path,
							const std::string& bus = AudioBus::kSFX,
							float volume = 1.0f, float pitch = 1.0f);

	/**
	 * Plays a 2D (non-spatialized) sound with full playback options, including
	 * looping, streaming from disk, stereo panning, and fade-in.
	 *
	 * Non-looping sounds that are not paused are automatically retired when
	 * they reach the end of playback.
	 *
	 * @param path Path to the audio file (WAV, MP3, or FLAC).
	 * @param options Playback configuration (bus, volume, pitch, pan, loop, stream, fade).
	 * @return Handle to the sound instance, or a null AudioHandle on failure.
	 */
	AudioHandle play2D(const std::string& path,
					   const AudioPlayOptions& options = {});

	/**
	 * Plays a music track streamed directly from disk on the Music bus.
	 * Avoids decoding large audio files into RAM.
	 *
	 * @param path Path to the music file (WAV, MP3, or FLAC).
	 * @param loop Whether to loop the track seamlessly (default true).
	 * @param fadeInSeconds Optional fade-in duration in seconds (default 0.0).
	 * @param volume Linear gain multiplier (default 1.0).
	 * @return Handle to the music stream, or a null AudioHandle on failure.
	 */
	AudioHandle playMusic(const std::string& path, bool loop = true,
						  float fadeInSeconds = 0.0f, float volume = 1.0f);

	// ----------------------------------------------------------- 3D Playback

	/**
	 * Plays a fire-and-forget 3D spatialized sound at a world-space position.
	 * Uses default inverse-distance attenuation and cleans up automatically
	 * when playback completes.
	 *
	 * @param path Path to the audio file (WAV, MP3, or FLAC).
	 * @param position World-space position of the sound.
	 * @param bus Target mixing bus (defaults to AudioBus::kSFX).
	 * @param volume Linear gain multiplier (default 1.0).
	 * @param pitch Pitch multiplier (default 1.0).
	 * @return Handle to the playing sound, or a null AudioHandle on failure.
	 */
	AudioHandle playOneShot3D(const std::string& path, const Vector3& position,
							  const std::string& bus = AudioBus::kSFX,
							  float volume = 1.0f, float pitch = 1.0f);

	/**
	 * Plays a 3D spatialized sound with full attenuation, Doppler, cone, and
	 * playback options.
	 *
	 * @param path Path to the audio file (WAV, MP3, or FLAC).
	 * @param position Initial world-space position of the sound.
	 * @param spatial Distance attenuation, Doppler, and directional cone settings.
	 * @param options Playback options (bus, volume, pitch, loop, stream, fade).
	 * @return Handle to the sound instance, or a null AudioHandle on failure.
	 */
	AudioHandle play3D(const std::string& path, const Vector3& position,
					   const Audio3DSettings& spatial = {},
					   const AudioPlayOptions& options = {});

	// -------------------------------------------------- Sound Handle Control

	/** Returns true if the handle references a live (playing or paused) sound. */
	bool isValid(AudioHandle handle) const;

	/** Returns true if the handle references a sound that is actively playing. */
	bool isPlaying(AudioHandle handle) const;

	/** Returns true if the handle references a sound that is currently paused. */
	bool isPaused(AudioHandle handle) const;

	/** Pauses a playing sound; retains its current playback cursor. */
	void pause(AudioHandle handle);

	/** Resumes a paused sound from where it left off. */
	void resume(AudioHandle handle);

	/** Immediately stops and releases a sound instance. */
	void stop(AudioHandle handle);

	/**
	 * Fades a sound's volume to zero over fadeOutSeconds and stops it when
	 * the fade completes. If fadeOutSeconds <= 0, stops immediately.
	 *
	 * @param handle Sound handle to fade out.
	 * @param fadeOutSeconds Duration of the fade-out in seconds.
	 */
	void stopWithFade(AudioHandle handle, float fadeOutSeconds);

	/**
	 * Fades a sound's volume smoothly from 0 to its configured volume over
	 * fadeInSeconds. If the sound is paused, resumes it.
	 *
	 * @param handle Sound handle to fade in.
	 * @param fadeInSeconds Duration of the fade-in in seconds.
	 */
	void fadeIn(AudioHandle handle, float fadeInSeconds);

	/**
	 * Fades a sound's volume smoothly from its current volume to targetVolume
	 * over durationSeconds.
	 *
	 * @param handle Sound handle to fade.
	 * @param targetVolume Target linear gain (>= 0.0).
	 * @param durationSeconds Duration of the transition in seconds.
	 */
	void fadeTo(AudioHandle handle, float targetVolume, float durationSeconds);

	/** Sets the linear volume of an active sound (clamped to >= 0.0). */
	void setVolume(AudioHandle handle, float volume);

	/** Returns the configured linear volume of a sound (0.0 for stale handles). */
	float getVolume(AudioHandle handle) const;

	/** Sets the pitch multiplier of an active sound (clamped to >= 0.01). */
	void setPitch(AudioHandle handle, float pitch);

	/** Returns the configured pitch multiplier of a sound (1.0 for stale handles). */
	float getPitch(AudioHandle handle) const;

	/** Sets the stereo pan of a 2D sound in [-1.0 (left), 1.0 (right)]. */
	void setPan(AudioHandle handle, float pan);

	/** Enables or disables seamless looping on an active sound. */
	void setLooping(AudioHandle handle, bool looping);

	/** Returns true if the sound is configured to loop. */
	bool isLooping(AudioHandle handle) const;

	/** Updates the world-space position of a 3D sound. */
	void setPosition(AudioHandle handle, const Vector3& position);

	/** Updates the world-space velocity (units/sec) of a 3D sound for Doppler. */
	void setVelocity(AudioHandle handle, const Vector3& velocity);

	/** Updates the world-space forward direction of a directional 3D sound. */
	void setDirection(AudioHandle handle, const Vector3& direction);

	/** Updates all 3D attenuation, cone, and Doppler parameters on an active sound. */
	void setSpatialSettings(AudioHandle handle, const Audio3DSettings& settings);

	/** Immediately stops and releases all currently active sounds. */
	void stopAll();

	/** Returns the number of live sound instances managed by AudioSystem. */
	size_t getActiveSoundCount() const;

	// ------------------------------------------------------ Listener Control

	/**
	 * Sets the 3D audio listener transform directly.
	 *
	 * Note: When an active AudioListenerComponent is present in the scene,
	 * tick() will overwrite the listener transform from that component every frame.
	 *
	 * @param position World-space position of the listener.
	 * @param forward World-space forward direction (right-handed; default (0, 0, -1)).
	 * @param up World-space up vector (default (0, 1, 0)).
	 * @param velocity World-space velocity in units/sec for Doppler (default (0, 0, 0)).
	 */
	void setListenerTransform(const Vector3& position, const Vector3& forward,
							  const Vector3& up,
							  const Vector3& velocity = Vector3(0.0f));

	/** Returns the current world-space position of the 3D listener. */
	Vector3 getListenerPosition() const;
	/** Returns the current world-space forward direction of the 3D listener. */
	Vector3 getListenerForward() const;
	/** Returns the current world-space up vector of the 3D listener. */
	Vector3 getListenerUp() const;
	/** Returns the current world-space velocity of the 3D listener. */
	Vector3 getListenerVelocity() const;

	/**
	 * Configures the listener's directional hearing cone.
	 *
	 * @param innerAngleRad Inner cone angle in radians (full gain inside).
	 * @param outerAngleRad Outer cone angle in radians.
	 * @param outerGain Gain multiplier outside the outer cone in [0, 1].
	 */
	void setListenerCone(float innerAngleRad, float outerAngleRad,
						 float outerGain);

	// --------------------------------------------- Offline / Engine Internal

	/**
	 * Pulls interleaved float32 PCM frames directly from the mixer graph.
	 * Intended for no-device mode, offline rendering, and automated tests.
	 *
	 * @param outFrames Destination buffer of at least frameCount * getChannels() floats.
	 * @param frameCount Number of PCM frames to render.
	 * @return Number of frames actually read (0 if uninitialized).
	 */
	BIRD_NO_EXPORT uint64_t readPcmFrames(float* outFrames, uint64_t frameCount);

	/** Registers an AudioSourceComponent for per-frame 3D transform syncing. */
	void _registerSource(AudioSourceComponent* source);
	/** Unregisters an AudioSourceComponent. */
	void _unregisterSource(AudioSourceComponent* source);
	/** Registers an AudioListenerComponent. */
	void _registerListener(AudioListenerComponent* listener);
	/** Unregisters an AudioListenerComponent. */
	void _unregisterListener(AudioListenerComponent* listener);
	/** Promotes the given listener component to be the active scene listener. */
	void _setActiveListener(AudioListenerComponent* listener);

   private:
	struct Impl;

	AudioHandle playInternal(const std::string& path, bool spatialized,
							 const Vector3& position,
							 const Audio3DSettings& spatial,
							 const AudioPlayOptions& options, bool oneShot);
	float computeEffectiveBusPitch(const std::string& busName) const;
	void refreshSoundPitches();
	void releaseSlot(uint32_t index);
	std::string resolveAudioPath(const std::string& path) const;

	_own<Impl> m_impl;
};

}  // namespace bird

#endif	// BIRD_AUDIO_AUDIOSYSTEM_H
