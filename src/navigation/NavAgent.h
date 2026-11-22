#ifndef BIRD_NAVAGENT_H
#define BIRD_NAVAGENT_H

#include <cstdint>

#include "../scene/components/Component.h"
#include "../util/Constants.h"
#include "../util/Math.h"
#include "NavTypes.h"

namespace bird {

class NavWorld;

/**
 * AI navigation component that steers its host WorldObject smoothly along a
 * NavMesh using Detour Crowd.
 *
 * Attach a NavAgent to any WorldObject in a Scene that has a NavMesh assigned
 * (`scene->getNavWorld()->setNavMesh(...)`). Call `setDestination(target)` to
 * plan and follow a path; each frame, NavWorld::step() advances the Detour
 * Crowd agent with configurable speed, acceleration, stopping distance, and
 * local obstacle avoidance, writing the smoothed position (and optional yaw
 * orientation) back to the WorldObject.
 *
 * Moving the WorldObject directly (via `setWorldPosition()`) is detected via
 * `WorldObject::getTransformRevision()` and automatically teleports the crowd
 * agent to the new position.
 */
class NavAgent : public Component {
   public:
	/**
	 * Constructs a NavAgent with the given movement and steering settings.
	 * @param settings Agent dimensions, speed, acceleration, and stopping distance.
	 * @throws std::invalid_argument if radius, height, maxSpeed, or maxAcceleration
	 *         are non-positive, or if stoppingDistance or rotationSpeed are negative.
	 */
	explicit NavAgent(const NavAgentSettings& settings = {});
	~NavAgent() override;

	void init() override {}
	void deinit() override {}
	void _init() override;
	void _deinit() override;

	// ---- Target & Path Control ----

	/**
	 * Commands the agent to navigate toward `target` on the scene's NavMesh.
	 *
	 * Projects `target` onto the nearest walkable NavMesh polygon and submits
	 * a Detour Crowd move request. If the agent has not yet joined a NavWorld
	 * with a valid NavMesh, the destination is stored as pending and applied
	 * automatically once a NavMesh becomes available.
	 *
	 * @param target Desired world-space destination.
	 * @return True if the target was accepted (or queued while awaiting a NavMesh),
	 *         false if a NavMesh is active but no walkable polygon exists near `target`.
	 */
	bool setDestination(const Vector3& target);

	/**
	 * Halts navigation immediately, clearing the active destination and zeroing
	 * crowd movement velocity.
	 */
	void stop();

	/**
	 * @return True if the agent has reached its last requested destination
	 *         (within `stoppingDistance`), false while travelling or if stopped
	 *         before arrival.
	 */
	bool hasReachedDestination() const { return m_reachedDestination; }

	/** @return True while the agent is actively pursuing a destination. */
	bool hasDestination() const { return m_hasDestination; }

	/** @return True when the agent currently has a non-negligible velocity. */
	bool isMoving() const;

	/** @return The last destination passed to setDestination(). */
	Vector3 getDestination() const { return m_destination; }

	/** @return Current world-space velocity vector (m/s) from Detour Crowd. */
	Vector3 getVelocity() const;

	/**
	 * @return Straight-line distance in metres from the agent's current position
	 *         to its active destination (0.0f if no destination is active).
	 */
	float getRemainingDistance() const;

	/**
	 * @return Remaining corner waypoints along the agent's active Detour Crowd
	 *         path corridor (empty if idle or not in a crowd).
	 */
	NavPath getCurrentPath() const;

	// ---- Runtime Parameter Configuration ----

	/** Sets maximum movement speed in m/s. @throws std::invalid_argument if <= 0. */
	void setMaxSpeed(float speed);
	float getMaxSpeed() const { return m_settings.maxSpeed; }

	/** Sets maximum acceleration in m/s^2. @throws std::invalid_argument if <= 0. */
	void setMaxAcceleration(float acceleration);
	float getMaxAcceleration() const { return m_settings.maxAcceleration; }

	/** Sets arrival stopping distance in metres. @throws std::invalid_argument if < 0. */
	void setStoppingDistance(float distance);
	float getStoppingDistance() const { return m_settings.stoppingDistance; }

	/** Sets agent collision radius in metres. @throws std::invalid_argument if <= 0. */
	void setRadius(float radius);
	float getRadius() const { return m_settings.radius; }

	/** Sets agent height in metres. @throws std::invalid_argument if <= 0. */
	void setHeight(float height);
	float getHeight() const { return m_settings.height; }

	/** Enables or disables automatic yaw rotation toward movement direction. */
	void setUpdateRotation(bool enabled) { m_settings.updateRotation = enabled; }
	bool getUpdateRotation() const { return m_settings.updateRotation; }

	/** Sets angular turn speed in radians/sec. @throws std::invalid_argument if < 0. */
	void setRotationSpeed(float speed);
	float getRotationSpeed() const { return m_settings.rotationSpeed; }

	/** @return Current agent settings snapshot. */
	const NavAgentSettings& getSettings() const { return m_settings; }

	/** @return True while registered in a NavWorld. */
	bool isInWorld() const { return m_world != nullptr; }

	/** @return The owning NavWorld, or nullptr if not yet initialized in a scene. */
	NavWorld* getWorld() const { return m_world; }

	// ---- Engine-Internal Crowd Sync ----

	BIRD_NO_EXPORT int _crowdIndex() const { return m_crowdIndex; }
	BIRD_NO_EXPORT void _setCrowdIndex(int idx) { m_crowdIndex = idx; }
	BIRD_NO_EXPORT void _detachWorld();
	BIRD_NO_EXPORT uint64_t _lastSyncedRevision() const { return m_lastSyncedRevision; }
	BIRD_NO_EXPORT void _setLastSyncedRevision(uint64_t rev) { m_lastSyncedRevision = rev; }
	BIRD_NO_EXPORT bool _hasPendingDestination() const { return m_pendingDestination; }
	BIRD_NO_EXPORT void _applyPendingDestination();
	BIRD_NO_EXPORT void _markReachedDestination();

   private:
	void pushParamsToCrowd();

	NavAgentSettings m_settings;
	NavWorld* m_world = nullptr;
	int m_crowdIndex = -1;
	uint64_t m_lastSyncedRevision = 0;

	Vector3 m_destination{0.0f};
	bool m_hasDestination = false;
	bool m_pendingDestination = false;
	bool m_reachedDestination = false;
};

}  // namespace bird

#endif	// !BIRD_NAVAGENT_H
