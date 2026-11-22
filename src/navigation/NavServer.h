#ifndef BIRD_NAVSERVER_H
#define BIRD_NAVSERVER_H

#include <vector>

#include "../util/Constants.h"

namespace bird {

class NavWorld;

/**
 * Process-wide navigation state and registry of active NavWorld instances.
 *
 * Mirrors PhysicsServer: exactly one NavServer exists at a time. Application
 * owns it during normal engine execution, and unit tests may construct a
 * standalone NavServer without creating a window or graphics pipeline.
 *
 * Every NavWorld registers itself on construction and unregisters on
 * destruction. All NavWorlds must be destroyed before the NavServer.
 */
class NavServer {
   public:
	/**
	 * Creates the active NavServer instance.
	 * @throws std::logic_error if another NavServer is already alive.
	 */
	NavServer();

	/**
	 * Destroys the NavServer. Aborts in debug/release if any NavWorld is still
	 * registered (matching PhysicsServer's lifetime guarantee).
	 */
	~NavServer();

	NavServer(const NavServer&) = delete;
	NavServer& operator=(const NavServer&) = delete;

	/** @return Pointer to the currently active NavServer, or nullptr if none exists. */
	static NavServer* get();

	/**
	 * Advances crowd simulation and agent movement in every registered NavWorld.
	 * @param deltaTime Elapsed time since the previous frame in seconds.
	 */
	void step(float deltaTime);

	/** @return Read-only list of all currently registered NavWorlds. */
	const std::vector<NavWorld*>& getWorlds() const { return m_worlds; }

	/** Engine-internal registration hooks called by NavWorld constructor/destructor. */
	BIRD_NO_EXPORT void _registerWorld(NavWorld* world);
	BIRD_NO_EXPORT void _unregisterWorld(NavWorld* world);

   private:
	std::vector<NavWorld*> m_worlds;
};

}  // namespace bird

#endif	// !BIRD_NAVSERVER_H
