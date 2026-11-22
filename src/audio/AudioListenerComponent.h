#ifndef BIRD_AUDIO_AUDIOLISTENERCOMPONENT_H
#define BIRD_AUDIO_AUDIOLISTENERCOMPONENT_H

#include "../scene/components/Component.h"
#include "../util/Math.h"

namespace bird {

class AudioSystem;

/**
 * Spatial audio listener component that drives the 3D audio engine's ears.
 *
 * Typically attached to the active Camera (or player head WorldObject).
 * Every frame during AudioSystem::tick(), the active listener synchronizes
 * its host WorldObject's world position, forward direction (-Z), up vector (+Y),
 * and velocity (derived automatically from frame-to-frame displacement for
 * Doppler shift, unless overridden) to the audio engine.
 *
 * Only one AudioListenerComponent is active at a time. Attaching a new
 * AudioListenerComponent with active = true (the default) automatically
 * makes it the active listener.
 */
class AudioListenerComponent : public Component {
   public:
	/**
	 * Constructs an AudioListenerComponent.
	 * @param active If true (default), becomes the active listener upon init().
	 */
	explicit AudioListenerComponent(bool active = true);
	~AudioListenerComponent() override;

	void init() override {}
	void deinit() override {}
	void _init() override;
	void _deinit() override;

	/**
	 * Activates or deactivates this listener. When activated, any previously
	 * active AudioListenerComponent is automatically deactivated.
	 *
	 * @param active True to make this the active listener.
	 */
	void setActive(bool active);

	/** Returns true if this listener is currently driving the 3D audio engine. */
	bool isActive() const { return m_active; }

	/**
	 * Overrides automatic displacement-based velocity calculation with an
	 * explicit world-space velocity vector (units/sec) for Doppler effects.
	 *
	 * @param velocity Explicit listener velocity in world space.
	 */
	void setVelocityOverride(const Vector3& velocity);

	/** Re-enables automatic velocity calculation from WorldObject movement. */
	void clearVelocityOverride();

	/** Returns true if an explicit velocity override is currently active. */
	bool hasVelocityOverride() const { return m_hasVelocityOverride; }

	/**
	 * Configures the listener's directional hearing cone.
	 *
	 * @param innerAngleRad Inner cone angle in radians (full gain inside).
	 * @param outerAngleRad Outer cone angle in radians.
	 * @param outerGain Gain multiplier outside the outer cone in [0, 1].
	 */
	void setCone(float innerAngleRad, float outerAngleRad, float outerGain);

	/**
	 * Engine-internal hook called by AudioSystem::tick() every frame to push
	 * the host WorldObject's transform and velocity to the audio engine.
	 *
	 * @param delta Elapsed frame time in seconds.
	 */
	void _syncTransform(float delta);

	/**
	 * Engine-internal hook called when AudioSystem is shutting down or
	 * deactivating this listener.
	 */
	void _setActiveFlag(bool active) { m_active = active; }

	/** Engine-internal hook called if AudioSystem shuts down before this component. */
	void _forgetSystem() { m_system = nullptr; }

   private:
	AudioSystem* m_system = nullptr;
	bool m_active = true;
	bool m_hasPrevPosition = false;
	Vector3 m_prevPosition{0.0f, 0.0f, 0.0f};
	bool m_hasVelocityOverride = false;
	Vector3 m_velocityOverride{0.0f, 0.0f, 0.0f};
	bool m_hasCone = false;
	float m_coneInner = 6.2831853f;
	float m_coneOuter = 6.2831853f;
	float m_coneOuterGain = 0.0f;
};

}  // namespace bird

#endif	// BIRD_AUDIO_AUDIOLISTENERCOMPONENT_H
