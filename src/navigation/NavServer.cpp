#include "NavServer.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "NavWorld.h"

namespace bird {

namespace {
NavServer* s_activeNavServer = nullptr;
}

NavServer::NavServer() {
	if (s_activeNavServer != nullptr) {
		throw std::logic_error("NavServer: another NavServer is already alive");
	}
	s_activeNavServer = this;
}

NavServer::~NavServer() {
	if (!m_worlds.empty()) {
		std::fprintf(stderr,
					 "NavServer destroyed while %zu NavWorld(s) are still alive.\n",
					 m_worlds.size());
		std::abort();
	}
	if (s_activeNavServer == this) {
		s_activeNavServer = nullptr;
	}
}

NavServer* NavServer::get() {
	return s_activeNavServer;
}

void NavServer::step(float deltaTime) {
	if (!(deltaTime > 0.0f)) return;
	for (NavWorld* world : m_worlds) {
		world->step(deltaTime);
	}
}

void NavServer::_registerWorld(NavWorld* world) {
	m_worlds.push_back(world);
}

void NavServer::_unregisterWorld(NavWorld* world) {
	m_worlds.erase(std::remove(m_worlds.begin(), m_worlds.end(), world),
				   m_worlds.end());
}

}  // namespace bird
