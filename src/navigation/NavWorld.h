#ifndef BIRD_NAVWORLD_H
#define BIRD_NAVWORLD_H

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"
#include "../util/Memory.hpp"
#include "NavMesh.h"
#include "NavTypes.h"

namespace bird {

class DebugDrawList;
class NavAgent;
class NavServer;
struct NavWorldImpl;

/**
 * Configuration for a per-scene NavWorld's Detour Crowd instance.
 */
struct NavWorldSettings {
	/** Maximum number of simultaneous NavAgent components in this world. */
	int maxAgents = 128;
	/** Upper bound on any agent's radius in metres (used by Detour Crowd proximity grid). */
	float maxAgentRadius = 2.0f;
};

/**
 * Per-scene navigation space pairing a baked NavMesh with a Detour Crowd
 * simulation for NavAgent components.
 *
 * Mirrors PhysicsWorld: owned by a Scene (accessed lazily via
 * `Scene::getNavWorld()` or isolated via `Scene::createNavWorld()`).
 *
 * Typical workflow:
 * 1. Bake or load a NavMesh (`NavMesh::buildFromScene()` or `NavMesh::loadFromFile()`).
 * 2. Assign it to the scene's world via `scene->getNavWorld()->setNavMesh(navMesh)`.
 * 3. Attach `NavAgent` components to entities and call `agent->setDestination(target)`.
 * 4. Each frame, `Application::tick()` steps `NavServer`, which updates Detour Crowd
 *    and writes smooth positions (and optional yaw rotations) back to each entity.
 */
class NavWorld {
   public:
	/**
	 * Creates a NavWorld and registers it with `server`.
	 * @param server The owning process-wide NavServer.
	 * @param settings Crowd capacity and maximum agent radius.
	 * @throws std::invalid_argument if maxAgents <= 0 or maxAgentRadius <= 0.
	 */
	explicit NavWorld(NavServer& server, const NavWorldSettings& settings = {});

	/** Detaches any remaining NavAgents and unregisters from the NavServer. */
	~NavWorld();

	NavWorld(const NavWorld&) = delete;
	NavWorld& operator=(const NavWorld&) = delete;

	/**
	 * Assigns (or replaces) the active NavMesh for this world.
	 *
	 * Rebuilds the internal Detour Crowd against the new NavMesh and
	 * automatically re-registers all currently attached NavAgents. Passing
	 * nullptr clears the NavMesh and pauses crowd simulation.
	 *
	 * @param navMesh Shared reference to a valid baked/loaded NavMesh, or nullptr.
	 */
	void setNavMesh(_ref<NavMesh> navMesh);

	/** @return The active NavMesh, or nullptr if none is assigned. */
	_ref<NavMesh> getNavMesh() const { return m_navMesh; }

	/** @return True when a valid NavMesh is currently assigned. */
	bool hasNavMesh() const;

	// ---- Pathfinding Query Shortcuts ----

	/**
	 * Computes a smooth 3D path on the active NavMesh from `start` to `end`.
	 * Convenience wrapper around `getNavMesh()->findPath()`.
	 *
	 * @param start World-space start position.
	 * @param end World-space goal position.
	 * @param searchExtents Half-extents of the polygon search box.
	 * @return NavPath (empty/invalid if no NavMesh is set or no path exists).
	 */
	NavPath findPath(const Vector3& start, const Vector3& end,
					 const Vector3& searchExtents = Vector3(2.0f, 4.0f, 2.0f)) const;

	/**
	 * Finds the nearest point on the active NavMesh surface.
	 * Convenience wrapper around `getNavMesh()->findNearestPoint()`.
	 *
	 * @param position World-space query position.
	 * @param searchExtents Half-extents of the polygon search box.
	 * @return Nearest NavPoint, or std::nullopt if no NavMesh is set or no polygon is in range.
	 */
	std::optional<NavPoint> findNearestPoint(
		const Vector3& position,
		const Vector3& searchExtents = Vector3(2.0f, 4.0f, 2.0f)) const;

	// ---- Simulation & Debug Visualization ----

	/**
	 * Advances the Detour Crowd simulation by `deltaTime` seconds and
	 * synchronizes transforms between WorldObjects and crowd agents.
	 *
	 * @param deltaTime Frame delta time in seconds (non-positive values are ignored).
	 */
	void step(float deltaTime);

	/** @return Number of NavAgent components currently registered in this world. */
	size_t getAgentCount() const { return m_agents.size(); }

	/**
	 * Appends NavMesh wireframes (cached layer) and active agent paths/bounds
	 * (transient lines) to the given 3D DebugDrawList.
	 *
	 * @param out Target DebugDrawList (typically GraphicsPipeline::getDebugDraw()).
	 * @param settings Visualisation toggles and colours.
	 */
	void debugDraw(DebugDrawList& out, const NavDebugSettings& settings = {}) const;

	/** Engine-internal accessors used by NavAgent. */
	BIRD_NO_EXPORT NavWorldImpl& _impl() { return *m_impl; }
	BIRD_NO_EXPORT const NavWorldImpl& _impl() const { return *m_impl; }
	BIRD_NO_EXPORT void _addAgent(NavAgent* agent);
	BIRD_NO_EXPORT void _removeAgent(NavAgent* agent);

   private:
	void rebuildCrowd();
	void registerAgentInCrowd(NavAgent* agent);
	void unregisterAgentFromCrowd(NavAgent* agent);

	NavServer& m_server;
	_ref<NavMesh> m_navMesh;
	std::unique_ptr<NavWorldImpl> m_impl;
	std::vector<NavAgent*> m_agents;
};

}  // namespace bird

#endif	// !BIRD_NAVWORLD_H
