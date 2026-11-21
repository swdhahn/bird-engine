#include "PhysicsServer.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "PhysicsInternal.h"
#include "PhysicsWorld.h"

namespace bird {

namespace {
PhysicsServer* s_instance = nullptr;

constexpr JPH::uint kMaxPhysicsJobs = 2048;
constexpr JPH::uint kMaxPhysicsBarriers = 8;
}  // namespace

PhysicsServer::PhysicsServer(const PhysicsServerSettings& settings) {
	if (s_instance != nullptr) {
		throw std::logic_error("PhysicsServer: only one instance may exist");
	}
	if (settings.tempAllocatorBytes == 0) {
		throw std::invalid_argument("PhysicsServer: tempAllocatorBytes must be positive");
	}

	JPH::RegisterDefaultAllocator();
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	int threads = settings.workerThreads;
	if (threads < 0) {
		threads = std::max(0, int(std::thread::hardware_concurrency()) - 1);
	}
	m_impl = std::make_unique<PhysicsServerImpl>();
	m_impl->tempAllocator =
		std::make_unique<JPH::TempAllocatorImplWithMallocFallback>(settings.tempAllocatorBytes);
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
	// Without threads (browsers lacking SharedArrayBuffer) the pool's
	// semaphores could never be signalled; run jobs inline instead.
	(void)threads;
	m_impl->jobSystem =
		std::make_unique<JPH::JobSystemSingleThreaded>(kMaxPhysicsJobs);
#else
	m_impl->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
		kMaxPhysicsJobs, kMaxPhysicsBarriers, threads);
#endif

	s_instance = this;
}

PhysicsServer::~PhysicsServer() {
	if (!m_worlds.empty()) {
		std::cerr << "PhysicsServer destroyed while " << m_worlds.size()
				  << " PhysicsWorld(s) are alive; destroy scenes first." << std::endl;
		std::abort();
	}
	// Threads and scratch memory first, then Jolt's global registry.
	m_impl.reset();
	JPH::UnregisterTypes();
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;
	s_instance = nullptr;
}

PhysicsServer* PhysicsServer::get() {
	return s_instance;
}

void PhysicsServer::step(float fixedDelta) {
	// Indexed: a contact callback may create a scene with its own world.
	for (size_t i = 0; i < m_worlds.size(); i++) {
		m_worlds[i]->step(fixedDelta);
	}
}

void PhysicsServer::_registerWorld(PhysicsWorld* world) {
	m_worlds.push_back(world);
}

void PhysicsServer::_unregisterWorld(PhysicsWorld* world) {
	m_worlds.erase(std::remove(m_worlds.begin(), m_worlds.end(), world), m_worlds.end());
}

}  // namespace bird
