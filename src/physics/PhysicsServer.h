#ifndef BIRD_PHYSICSSERVER_H
#define BIRD_PHYSICSSERVER_H

#include <cstdint>
#include <memory>
#include <vector>

namespace bird {

class PhysicsWorld;
struct PhysicsServerImpl;

struct PhysicsServerSettings {
	/** Scratch arena for one simulation step, in bytes (overflow uses malloc). */
	uint32_t tempAllocatorBytes = 32 * 1024 * 1024;
	/** Worker threads; negative means hardware threads minus one. */
	int workerThreads = -1;
};

/**
 * Process-wide physics state: Jolt's allocator, type registry, step scratch
 * memory, worker threads and collision filters. Exactly one may exist at a
 * time (Jolt's registry is global); the Application owns it, and tests can
 * create their own without a window.
 *
 * Every PhysicsWorld registers here and must be destroyed before the server.
 */
class PhysicsServer {
   public:
	/** @throws std::logic_error when another PhysicsServer is alive. */
	explicit PhysicsServer(const PhysicsServerSettings& settings = {});
	/** Aborts when worlds are still alive (they would dangle). */
	~PhysicsServer();

	PhysicsServer(const PhysicsServer&) = delete;
	PhysicsServer& operator=(const PhysicsServer&) = delete;

	/** The live server, or nullptr. */
	static PhysicsServer* get();

	/** Steps every registered world once, in creation order. */
	void step(float fixedDelta);

	const std::vector<PhysicsWorld*>& getWorlds() const { return m_worlds; }

	/** Engine-internal. */
	PhysicsServerImpl& _impl() { return *m_impl; }
	void _registerWorld(PhysicsWorld* world);
	void _unregisterWorld(PhysicsWorld* world);

   private:
	std::unique_ptr<PhysicsServerImpl> m_impl;
	std::vector<PhysicsWorld*> m_worlds;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSSERVER_H
