#include "ParticleEmitter.h"

#include <algorithm>
#include <cmath>

#include "../../Application.h"
#include "../../renders/GraphicsPipeline.h"

namespace bird {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
}  // namespace

ParticleEmitter::ParticleEmitter(size_t maxParticles) {
	m_object = nullptr;
	setColorGradient(Vector4(1.0f, 1.0f, 1.0f, 1.0f),
					 Vector4(1.0f, 1.0f, 1.0f, 0.0f));
	setSizeRange(0.25f, 0.05f);
	setCapacity(maxParticles);
}

void ParticleEmitter::init() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->addParticleEmitter(this);
	}
}

void ParticleEmitter::deinit() {
	if (APPLICATION && APPLICATION->getGraphicsPipeline()) {
		APPLICATION->getGraphicsPipeline()->removeParticleEmitter(this);
	}
}

void ParticleEmitter::process(float delta) {
	step(delta);
}

void ParticleEmitter::setCapacity(size_t maxParticles) {
	const size_t safeCap = std::max<size_t>(1, maxParticles);
	m_pool.resize(safeCap);
	m_sortIndices.resize(safeCap);
	m_quadVertices.resize(safeCap * 4);
	m_activeCount = 0;
	m_worldBounds = AABB{};
}

void ParticleEmitter::reset() {
	m_activeCount = 0;
	m_elapsedTime = 0.0f;
	m_spawnAccumulator = 0.0f;
	m_worldBounds = AABB{};
	for (BurstState& bs : m_burstStates) {
		bs.fired = false;
	}
}

void ParticleEmitter::setDuration(float duration) {
	m_duration = std::max(duration, 1e-3f);
}

void ParticleEmitter::setSpawnRate(float particlesPerSecond) {
	m_spawnRate = std::max(particlesPerSecond, 0.0f);
}

void ParticleEmitter::addBurst(const ParticleBurst& burst) {
	ParticleBurst sanitized = burst;
	sanitized.time = std::max(sanitized.time, 0.0f);
	sanitized.maxCount = std::max(sanitized.minCount, sanitized.maxCount);
	m_bursts.push_back(sanitized);
	m_burstStates.push_back(BurstState{sanitized, false});
}

void ParticleEmitter::clearBursts() {
	m_bursts.clear();
	m_burstStates.clear();
}

void ParticleEmitter::setLifetimeRange(float minLifetime, float maxLifetime) {
	m_minLifetime = std::max(minLifetime, 1e-4f);
	m_maxLifetime = std::max(maxLifetime, m_minLifetime);
}

void ParticleEmitter::setInitialSpeedRange(float minSpeed, float maxSpeed) {
	m_minSpeed = std::max(minSpeed, 0.0f);
	m_maxSpeed = std::max(maxSpeed, m_minSpeed);
}

void ParticleEmitter::setEmissionCone(const Vector3& direction,
									  float coneHalfAngleRadians) {
	const float len = glm::length(direction);
	m_emissionDirection =
		len > 1e-5f ? (direction / len) : Vector3(0.0f, 1.0f, 0.0f);
	m_coneAngle = std::clamp(coneHalfAngleRadians, 0.0f, kPi);
}

void ParticleEmitter::setDrag(float drag) {
	m_drag = std::max(drag, 0.0f);
}

void ParticleEmitter::setColorGradient(const Vector4& startColor,
									   const Vector4& endColor) {
	m_colorOverLife.clear();
	m_colorOverLife.addKey(0.0f, startColor);
	m_colorOverLife.addKey(1.0f, endColor);
}

void ParticleEmitter::setSizeRange(float startSize, float endSize) {
	m_sizeOverLife.clear();
	m_sizeOverLife.addKey(0.0f, std::max(startSize, 0.0f));
	m_sizeOverLife.addKey(1.0f, std::max(endSize, 0.0f));
}

size_t ParticleEmitter::emitBurst(uint32_t count) {
	const size_t available = m_pool.size() - m_activeCount;
	const size_t toSpawn = std::min<size_t>(count, available);
	if (toSpawn == 0) return 0;

	const Vector3 origin =
		m_localSpace ? m_positionOffset : emitterWorldOrigin();
	const Matrix3 basis =
		m_localSpace ? Matrix3(1.0f) : emitterWorldBasis();
	const Vector3 worldOffset =
		m_localSpace ? (emitterWorldOrigin() - m_positionOffset) : Vector3(0.0f);

	for (size_t i = 0; i < toSpawn; i++) {
		spawnOne(origin, basis);
		const Particle& p = m_pool[m_activeCount - 1];
		const Vector3 wp = p.position + worldOffset;
		const Vector3 halfExt(p.size * 0.5f);
		m_worldBounds.expand(wp - halfExt);
		m_worldBounds.expand(wp + halfExt);
	}
	return toSpawn;
}

void ParticleEmitter::step(float dt) {
	if (dt <= 0.0f || !std::isfinite(dt)) return;

	const Vector3 simOrigin =
		m_localSpace ? m_positionOffset : emitterWorldOrigin();
	const Matrix3 simBasis =
		m_localSpace ? Matrix3(1.0f) : emitterWorldBasis();

	// 1. Advance existing particles and recycle expired ones in O(1).
	const float dragFactor = 1.0f / (1.0f + m_drag * dt);
	size_t i = 0;
	while (i < m_activeCount) {
		Particle& p = m_pool[i];
		p.age += dt;
		if (p.age >= p.lifetime) {
			p = m_pool[m_activeCount - 1];
			m_activeCount--;
			continue;
		}
		p.velocity = (p.velocity + m_gravity * dt) * dragFactor;
		p.position += p.velocity * dt;
		p.rotation += p.angularVelocity * dt;
		evaluateParticleVisuals(p);
		i++;
	}

	// 2. Spawn scheduled bursts and continuous emission when playing.
	if (m_playing) {
		const float prevTime = m_elapsedTime;
		m_elapsedTime += dt;

		// Check bursts in [prevTime, m_elapsedTime].
		for (BurstState& bs : m_burstStates) {
			if (!bs.fired && m_elapsedTime >= bs.burst.time &&
				prevTime <= bs.burst.time) {
				bs.fired = true;
				uint32_t count = bs.burst.minCount;
				if (bs.burst.maxCount > bs.burst.minCount) {
					const float r = nextRandom01();
					count = bs.burst.minCount +
							static_cast<uint32_t>(
								r * static_cast<float>(bs.burst.maxCount -
													   bs.burst.minCount + 1));
					count = std::min(count, bs.burst.maxCount);
				}
				const size_t avail = m_pool.size() - m_activeCount;
				const size_t spawnCount = std::min<size_t>(count, avail);
				for (size_t s = 0; s < spawnCount; s++) {
					spawnOne(simOrigin, simBasis);
				}
			}
		}

		if (m_elapsedTime >= m_duration) {
			if (m_looping) {
				m_elapsedTime = std::fmod(m_elapsedTime, m_duration);
				for (BurstState& bs : m_burstStates) {
					bs.fired = false;
					if (bs.burst.time <= m_elapsedTime) {
						bs.fired = true;
						const size_t avail = m_pool.size() - m_activeCount;
						const size_t spawnCount =
							std::min<size_t>(bs.burst.minCount, avail);
						for (size_t s = 0; s < spawnCount; s++) {
							spawnOne(simOrigin, simBasis);
						}
					}
				}
			} else {
				m_playing = false;
			}
		}

		if (m_playing && m_spawnRate > 0.0f) {
			m_spawnAccumulator += m_spawnRate * dt;
			const size_t requested =
				static_cast<size_t>(std::floor(m_spawnAccumulator));
			m_spawnAccumulator -= static_cast<float>(requested);
			const size_t avail = m_pool.size() - m_activeCount;
			const size_t spawnCount = std::min(requested, avail);
			for (size_t s = 0; s < spawnCount; s++) {
				spawnOne(simOrigin, simBasis);
			}
		}
	}

	// 3. Recompute enclosing world-space AABB for frustum culling.
	m_worldBounds = AABB{};
	const Vector3 worldOffset =
		m_localSpace ? (emitterWorldOrigin() - m_positionOffset) : Vector3(0.0f);
	for (size_t idx = 0; idx < m_activeCount; idx++) {
		const Particle& p = m_pool[idx];
		const Vector3 wp = p.position + worldOffset;
		const Vector3 halfExt(std::max(p.size * 0.5f, 1e-3f));
		m_worldBounds.expand(wp - halfExt);
		m_worldBounds.expand(wp + halfExt);
	}
}

void ParticleEmitter::spawnOne(const Vector3& emitterOrigin,
							   const Matrix3& emitterBasis) {
	if (m_activeCount >= m_pool.size()) return;

	Particle& p = m_pool[m_activeCount++];
	p.position = emitterOrigin;
	p.age = 0.0f;
	p.lifetime = nextRandomRange(m_minLifetime, m_maxLifetime);
	p.rotation = 0.0f;
	p.angularVelocity = 0.0f;

	// Build orthonormal basis around m_emissionDirection.
	const Vector3 axis = glm::normalize(m_emissionDirection);
	const Vector3 helper =
		std::abs(axis.y) < 0.99f ? Vector3(0.0f, 1.0f, 0.0f)
								 : Vector3(1.0f, 0.0f, 0.0f);
	const Vector3 tangent = glm::normalize(glm::cross(helper, axis));
	const Vector3 bitangent = glm::cross(axis, tangent);

	// Uniform solid-angle sampling inside the cone of half-angle m_coneAngle.
	const float cosMax = std::cos(m_coneAngle);
	const float cosTheta =
		m_coneAngle <= 1e-5f ? 1.0f : (cosMax + nextRandom01() * (1.0f - cosMax));
	const float sinTheta =
		std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
	const float phi = nextRandom01() * kTwoPi;

	const Vector3 localDir =
		tangent * (std::cos(phi) * sinTheta) +
		bitangent * (std::sin(phi) * sinTheta) + axis * cosTheta;
	const Vector3 worldDir = glm::normalize(emitterBasis * localDir);
	const float speed = nextRandomRange(m_minSpeed, m_maxSpeed);
	p.velocity = worldDir * speed;

	evaluateParticleVisuals(p);
}

void ParticleEmitter::evaluateParticleVisuals(Particle& p) const {
	const float normalizedAge =
		p.lifetime > 0.0f ? std::clamp(p.age / p.lifetime, 0.0f, 1.0f) : 1.0f;
	p.color = m_colorOverLife.isEmpty() ? Vector4(1.0f)
										: m_colorOverLife.sample(normalizedAge);
	p.size = m_sizeOverLife.isEmpty()
				 ? 0.1f
				 : std::max(0.0f, m_sizeOverLife.sample(normalizedAge));
}

Vector3 ParticleEmitter::emitterWorldOrigin() const {
	if (m_object != nullptr) {
		const Matrix4& model = m_object->getTransformMatrix();
		return Vector3(model * Vector4(m_positionOffset, 1.0f));
	}
	return m_positionOffset;
}

Matrix3 ParticleEmitter::emitterWorldBasis() const {
	if (m_object != nullptr) {
		const Matrix3 raw(m_object->getTransformMatrix());
		const Vector3 c0 = glm::length(raw[0]) > 1e-5f ? glm::normalize(raw[0])
													   : Vector3(1, 0, 0);
		const Vector3 c1 = glm::length(raw[1]) > 1e-5f ? glm::normalize(raw[1])
													   : Vector3(0, 1, 0);
		const Vector3 c2 = glm::length(raw[2]) > 1e-5f ? glm::normalize(raw[2])
													   : Vector3(0, 0, 1);
		return Matrix3(c0, c1, c2);
	}
	return Matrix3(1.0f);
}

size_t ParticleEmitter::buildBillboardQuads(const Vector3& cameraRight,
											const Vector3& cameraUp,
											const Vector3& cameraForward) {
	if (m_activeCount == 0) return 0;

	for (size_t i = 0; i < m_activeCount; i++) {
		m_sortIndices[i] = static_cast<uint32_t>(i);
	}

	const Vector3 worldOffset =
		m_localSpace ? (emitterWorldOrigin() - m_positionOffset) : Vector3(0.0f);

	// Sort alpha-blended particles back-to-front along cameraForward so
	// overlapping translucent particles composite without popping.
	if (m_blendMode == ParticleBlendMode::Alpha && m_activeCount > 1) {
		std::sort(m_sortIndices.begin(),
				  m_sortIndices.begin() + static_cast<ptrdiff_t>(m_activeCount),
				  [this, &cameraForward, &worldOffset](uint32_t a, uint32_t b) {
					  const float da =
						  glm::dot(m_pool[a].position + worldOffset,
								   cameraForward);
					  const float db =
						  glm::dot(m_pool[b].position + worldOffset,
								   cameraForward);
					  return da > db;
				  });
	}

	for (size_t outIdx = 0; outIdx < m_activeCount; outIdx++) {
		const Particle& p = m_pool[m_sortIndices[outIdx]];
		const Vector3 center = p.position + worldOffset;
		const float halfSize = p.size * 0.5f;
		const Vector3 right = cameraRight * halfSize;
		const Vector3 up = cameraUp * halfSize;

		const size_t base = outIdx * 4;
		// 0: Bottom-Left
		m_quadVertices[base + 0] =
			ParticleQuadVertex{center - right - up, Vector2(0.0f, 0.0f), p.color};
		// 1: Bottom-Right
		m_quadVertices[base + 1] =
			ParticleQuadVertex{center + right - up, Vector2(1.0f, 0.0f), p.color};
		// 2: Top-Right
		m_quadVertices[base + 2] =
			ParticleQuadVertex{center + right + up, Vector2(1.0f, 1.0f), p.color};
		// 3: Top-Left
		m_quadVertices[base + 3] =
			ParticleQuadVertex{center - right + up, Vector2(0.0f, 1.0f), p.color};
	}

	return m_activeCount * 4;
}

float ParticleEmitter::nextRandom01() {
	// xorshift32: deterministic, fast, zero external state.
	uint32_t x = m_rngState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	m_rngState = x;
	return static_cast<float>(x & 0x00FFFFFFu) / 16777216.0f;
}

float ParticleEmitter::nextRandomRange(float minVal, float maxVal) {
	if (maxVal <= minVal) return minVal;
	return minVal + nextRandom01() * (maxVal - minVal);
}

}  // namespace bird
