#ifndef BIRD_PARTICLE_EMITTER_H
#define BIRD_PARTICLE_EMITTER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../../animation/Curve.h"
#include "../../renders/generic/Bounds.h"
#include "../../renders/generic/Texture.h"
#include "../../util/Math.h"
#include "ProcessScript.h"

namespace bird {

/** Blending modes used when drawing billboard particles in the forward pass. */
enum class ParticleBlendMode : uint8_t {
	/** Standard alpha blending: src * src.a + dst * (1 - src.a). */
	Alpha = 0,
	/** Premultiplied additive blending: src * src.a + dst (sparks, fire). */
	Additive = 1
};

/**
 * Scheduled burst of particles triggered once the emitter's playback clock
 * crosses `time` seconds.
 */
struct ParticleBurst {
	/** Elapsed emitter time in seconds when the burst fires (>= 0). */
	float time = 0.0f;
	/** Minimum number of particles spawned by the burst. */
	uint32_t minCount = 10;
	/** Maximum number of particles spawned by the burst (>= minCount). */
	uint32_t maxCount = 10;
};

/**
 * Interleaved CPU vertex for one corner of a camera-facing particle quad
 * (36 bytes, matching assets/shaders/particle.vert).
 */
struct ParticleQuadVertex {
	Vector3 position{0.0f};
	Vector2 uv{0.0f};
	Vector4 color{1.0f};
};
static_assert(sizeof(ParticleQuadVertex) == 36, "vertex layout drift");

/**
 * CPU-simulated billboard particle emitter component.
 *
 * Inherits from ProcessScriptComponent so it receives per-frame process(delta)
 * simulation ticks automatically, and registers with GraphicsPipeline during
 * init() so its active particles are rendered inside the forward transparency
 * pass with proper depth testing and alpha or additive blending.
 *
 * Memory model:
 *  - Preallocates a fixed-capacity particle pool and quad vertex buffer during
 *    construction or setCapacity() (called at setup time).
 *  - Performs **zero per-frame heap allocations** during simulation (process /
 *    step), burst emission, and billboard quad generation (buildBillboardQuads).
 *  - Active particles are stored contiguously in [0, m_activeCount); expired
 *    particles are recycled in O(1) via swap-with-last removal.
 */
class ParticleEmitter : public ProcessScriptComponent {
   public:
	/** Default maximum particles per emitter when not specified. */
	static constexpr size_t kDefaultCapacity = 256;

	/**
	 * Constructs a particle emitter with a fixed-capacity pool.
	 * @param maxParticles Maximum simultaneously active particles (>= 1).
	 *        Preallocates all simulation and vertex storage immediately.
	 */
	explicit ParticleEmitter(size_t maxParticles = kDefaultCapacity);
	~ParticleEmitter() override = default;

	/** Registers the emitter with the active GraphicsPipeline. */
	void init() override;

	/** Unregisters the emitter from the active GraphicsPipeline. */
	void deinit() override;

	/**
	 * Advances the particle simulation by delta seconds. Called automatically
	 * each frame by Application::tick() before rendering.
	 * @param delta Frame delta time in seconds.
	 */
	void process(float delta) override;

	/**
	 * Steps the emitter simulation by dt seconds (zero heap allocations).
	 * Can also be invoked directly in headless unit tests.
	 * @param dt Time step in seconds (ignored when <= 0).
	 */
	void step(float dt);

	/**
	 * Reallocates the fixed-capacity pool and resets active particles.
	 * Call during setup (not per-frame).
	 * @param maxParticles New maximum capacity (clamped to >= 1).
	 */
	void setCapacity(size_t maxParticles);

	/** @return Maximum number of particles the pool can hold simultaneously. */
	size_t getCapacity() const { return m_pool.size(); }

	/** @return Number of currently alive particles in [0, getCapacity()]. */
	size_t getActiveCount() const { return m_activeCount; }

	// ---- Playback & Emission Controls ----

	/** Starts or resumes continuous particle emission. */
	void play() { m_playing = true; }

	/** Pauses continuous and burst emission while existing particles finish. */
	void pause() { m_playing = false; }

	/**
	 * Stops emission, clears all active particles, and rewinds the emitter
	 * clock and burst triggers to t = 0.
	 */
	void reset();

	/** @return True when the emitter is actively spawning particles. */
	bool isPlaying() const { return m_playing; }

	/**
	 * Configures whether the emitter loops its duration cycle (re-arming
	 * scheduled bursts each cycle) or stops emitting after `duration` seconds.
	 * @param looping True to loop indefinitely (default).
	 */
	void setLooping(bool looping) { m_looping = looping; }
	bool isLooping() const { return m_looping; }

	/**
	 * Sets the length of one emission cycle in seconds (> 0).
	 * @param duration Cycle duration in seconds.
	 */
	void setDuration(float duration);
	float getDuration() const { return m_duration; }

	/** @return Current playback time within the emission cycle in seconds. */
	float getElapsedTime() const { return m_elapsedTime; }

	/**
	 * Sets continuous spawn rate in particles per second (>= 0).
	 * @param particlesPerSecond Spawn rate.
	 */
	void setSpawnRate(float particlesPerSecond);
	float getSpawnRate() const { return m_spawnRate; }

	/**
	 * Schedules a burst of particles at `time` seconds into the emission cycle.
	 * Setup-time method (reserves capacity in the burst schedule).
	 * @param burst Burst timing and count range.
	 */
	void addBurst(const ParticleBurst& burst);

	/** Convenience overload scheduling a fixed-count burst at `time` seconds. */
	void addBurst(float time, uint32_t count) {
		addBurst(ParticleBurst{time, count, count});
	}

	/** Removes all scheduled bursts. */
	void clearBursts();

	/** @return Read-only list of scheduled bursts. */
	const std::vector<ParticleBurst>& getBursts() const { return m_bursts; }

	/**
	 * Immediately spawns up to `count` particles at the emitter's current
	 * position, clamped to remaining pool capacity (zero heap allocations).
	 * @param count Requested number of particles to spawn.
	 * @return Actual number of particles spawned.
	 */
	size_t emitBurst(uint32_t count);

	// ---- Particle Initial State & Physics ----

	/**
	 * Sets the randomized particle lifetime range in seconds (> 0).
	 * @param minLifetime Minimum lifetime in seconds.
	 * @param maxLifetime Maximum lifetime in seconds.
	 */
	void setLifetimeRange(float minLifetime, float maxLifetime);
	float getMinLifetime() const { return m_minLifetime; }
	float getMaxLifetime() const { return m_maxLifetime; }

	/**
	 * Sets the randomized initial speed range in world units per second.
	 * @param minSpeed Minimum launch speed (>= 0).
	 * @param maxSpeed Maximum launch speed (>= minSpeed).
	 */
	void setInitialSpeedRange(float minSpeed, float maxSpeed);
	float getMinInitialSpeed() const { return m_minSpeed; }
	float getMaxInitialSpeed() const { return m_maxSpeed; }

	/**
	 * Sets the local-space emission axis and half-angle cone spread.
	 * A cone angle of 0 emits strictly along `direction`; pi emits omnidirectionally.
	 * @param direction Local emission direction vector (normalized internally).
	 * @param coneHalfAngleRadians Half-angle of the emission cone in [0, pi].
	 */
	void setEmissionCone(const Vector3& direction, float coneHalfAngleRadians);
	const Vector3& getEmissionDirection() const { return m_emissionDirection; }
	float getConeAngle() const { return m_coneAngle; }

	/**
	 * Sets constant world-space acceleration (gravity) in units/s^2.
	 * @param gravity World-space acceleration vector (e.g. (0, -9.81, 0)).
	 */
	void setGravity(const Vector3& gravity) { m_gravity = gravity; }
	const Vector3& getGravity() const { return m_gravity; }

	/**
	 * Sets linear velocity damping coefficient (>= 0, 1/s). Velocity decays
	 * each step by factor 1 / (1 + drag * dt).
	 * @param drag Damping factor (>= 0).
	 */
	void setDrag(float drag);
	float getDrag() const { return m_drag; }

	/**
	 * Sets whether particles simulate in world space (detached after spawn,
	 * leaving trails when the emitter moves) or local to the host WorldObject.
	 * @param localSpace True for local-space simulation, false for world-space.
	 */
	void setLocalSpace(bool localSpace) { m_localSpace = localSpace; }
	bool isLocalSpace() const { return m_localSpace; }

	/**
	 * Sets the base emitter origin when the component is not attached to a
	 * WorldObject (or as a local offset from the host WorldObject).
	 * @param offset Position offset in world/local units.
	 */
	void setPositionOffset(const Vector3& offset) { m_positionOffset = offset; }
	const Vector3& getPositionOffset() const { return m_positionOffset; }

	/**
	 * Seeds the deterministic internal PRNG (xorshift32) for reproducible
	 * VFX simulations and tests.
	 * @param seed Non-zero 32-bit seed (0 is remapped to 1).
	 */
	void setRandomSeed(uint32_t seed) { m_rngState = seed != 0 ? seed : 1u; }

	// ---- Curves Over Normalized Lifetime [0, 1] ----

	/**
	 * Assigns a custom RGBA color-over-life curve sampled at normalized age
	 * t in [0, 1].
	 * @param curve Keyframed Vector4 curve.
	 */
	void setColorOverLife(Curve<Vector4> curve) {
		m_colorOverLife = std::move(curve);
	}

	/**
	 * Convenience helper setting a two-key linear RGBA color gradient from
	 * birth (t = 0) to death (t = 1).
	 * @param startColor RGBA color at t = 0.
	 * @param endColor RGBA color at t = 1.
	 */
	void setColorGradient(const Vector4& startColor, const Vector4& endColor);

	const Curve<Vector4>& getColorOverLife() const { return m_colorOverLife; }

	/**
	 * Assigns a custom billboard size-over-life curve sampled at normalized
	 * age t in [0, 1].
	 * @param curve Keyframed float curve (world-space quad width/height).
	 */
	void setSizeOverLife(Curve<float> curve) {
		m_sizeOverLife = std::move(curve);
	}

	/**
	 * Convenience helper setting a two-key linear size ramp from birth (t = 0)
	 * to death (t = 1).
	 * @param startSize Quad size at t = 0.
	 * @param endSize Quad size at t = 1.
	 */
	void setSizeRange(float startSize, float endSize);

	const Curve<float>& getSizeOverLife() const { return m_sizeOverLife; }

	// ---- Rendering Appearance & Bounds ----

	/**
	 * Sets the forward-pass blending mode (Alpha or Additive).
	 * @param mode Blending mode.
	 */
	void setBlendMode(ParticleBlendMode mode) { m_blendMode = mode; }
	ParticleBlendMode getBlendMode() const { return m_blendMode; }

	/**
	 * Sets an optional sprite texture multiplied by particle vertex color.
	 * When nullptr, the shader renders a procedural soft circular radial falloff.
	 * @param texture Shared 2D sprite texture, or nullptr.
	 */
	void setTexture(std::shared_ptr<Texture> texture) {
		m_texture = std::move(texture);
	}
	const std::shared_ptr<Texture>& getTexture() const { return m_texture; }

	/**
	 * @return World-space AABB enclosing all currently active particles
	 *         (padded by each particle's half-size), or an empty AABB when
	 *         getActiveCount() == 0.
	 */
	const AABB& getWorldBounds() const { return m_worldBounds; }

	/**
	 * Populates the preallocated quad vertex buffer with camera-facing
	 * billboard vertices for all active particles (4 vertices per particle,
	 * zero heap allocations).
	 *
	 * @param cameraRight Normalized camera right (+X) vector in world space.
	 * @param cameraUp Normalized camera up (+Y) vector in world space.
	 * @param cameraForward Normalized camera forward (-Z) vector in world
	 *        space; used to back-to-front sort alpha-blended particles.
	 * @return Number of vertices written into getQuadVertices()
	 *         (4 * getActiveCount()).
	 */
	size_t buildBillboardQuads(const Vector3& cameraRight,
							   const Vector3& cameraUp,
							   const Vector3& cameraForward);

	/**
	 * @return Pointer to the internal preallocated quad vertex buffer
	 *         populated by buildBillboardQuads().
	 */
	const ParticleQuadVertex* getQuadVertices() const {
		return m_quadVertices.data();
	}

   private:
	struct Particle {
		Vector3 position{0.0f};
		Vector3 velocity{0.0f};
		Vector4 color{1.0f};
		float age = 0.0f;
		float lifetime = 1.0f;
		float size = 1.0f;
		float rotation = 0.0f;
		float angularVelocity = 0.0f;
	};

	struct BurstState {
		ParticleBurst burst;
		bool fired = false;
	};

	void spawnOne(const Vector3& emitterOrigin, const Matrix3& emitterBasis);
	void evaluateParticleVisuals(Particle& p) const;
	Vector3 emitterWorldOrigin() const;
	Matrix3 emitterWorldBasis() const;
	float nextRandom01();
	float nextRandomRange(float minVal, float maxVal);

	std::vector<Particle> m_pool;
	size_t m_activeCount = 0;

	// Preallocated sort index buffer and quad vertex buffer (zero per-frame allocs).
	std::vector<uint32_t> m_sortIndices;
	std::vector<ParticleQuadVertex> m_quadVertices;

	std::vector<ParticleBurst> m_bursts;
	std::vector<BurstState> m_burstStates;

	bool m_playing = true;
	bool m_looping = true;
	bool m_localSpace = false;
	float m_duration = 5.0f;
	float m_elapsedTime = 0.0f;
	float m_spawnRate = 20.0f;
	float m_spawnAccumulator = 0.0f;

	float m_minLifetime = 1.0f;
	float m_maxLifetime = 2.0f;
	float m_minSpeed = 1.0f;
	float m_maxSpeed = 3.0f;
	Vector3 m_emissionDirection{0.0f, 1.0f, 0.0f};
	float m_coneAngle = 0.35f;
	Vector3 m_gravity{0.0f, -9.81f, 0.0f};
	float m_drag = 0.1f;
	Vector3 m_positionOffset{0.0f};

	Curve<Vector4> m_colorOverLife;
	Curve<float> m_sizeOverLife;

	ParticleBlendMode m_blendMode = ParticleBlendMode::Alpha;
	std::shared_ptr<Texture> m_texture = nullptr;
	AABB m_worldBounds;
	uint32_t m_rngState = 0xA341316Cu;
};

}  // namespace bird

#endif	// BIRD_PARTICLE_EMITTER_H
