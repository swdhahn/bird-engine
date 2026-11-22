#include "NavWorld.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

#include "../renders/DebugDraw.h"
#include "../scene/WorldObject.h"
#include "NavAgent.h"
#include "NavInternal.h"
#include "NavServer.h"

namespace bird {

namespace {

dtCrowdAgentParams makeCrowdParams(const NavAgentSettings& s) {
	dtCrowdAgentParams ap{};
	ap.radius = s.radius;
	ap.height = s.height;
	ap.maxAcceleration = s.maxAcceleration;
	ap.maxSpeed = s.maxSpeed;
	ap.collisionQueryRange = s.radius * 12.0f;
	ap.pathOptimizationRange = s.radius * 30.0f;
	ap.separationWeight = s.separationWeight;
	ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS |
					 DT_CROWD_OPTIMIZE_TOPO | DT_CROWD_SEPARATION;
	if (s.obstacleAvoidance) {
		ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	}
	ap.obstacleAvoidanceType = 3;
	ap.queryFilterType = 0;
	ap.userData = nullptr;
	return ap;
}

void drawCylinderWireframe(DebugDrawList& out, const Vector3& base, float radius,
						   float height, const Vector4& color) {
	constexpr int kSegments = 12;
	constexpr float kTwoPi = 6.28318530718f;
	for (int i = 0; i < kSegments; ++i) {
		const float a0 = kTwoPi * static_cast<float>(i) / kSegments;
		const float a1 = kTwoPi * static_cast<float>(i + 1) / kSegments;
		const Vector3 b0(base.x + std::cos(a0) * radius, base.y,
						 base.z + std::sin(a0) * radius);
		const Vector3 b1(base.x + std::cos(a1) * radius, base.y,
						 base.z + std::sin(a1) * radius);
		const Vector3 t0 = b0 + Vector3(0.0f, height, 0.0f);
		const Vector3 t1 = b1 + Vector3(0.0f, height, 0.0f);
		out.line(b0, b1, color);
		out.line(t0, t1, color);
		if (i % 3 == 0) {
			out.line(b0, t0, color);
		}
	}
}

}  // namespace

NavWorld::NavWorld(NavServer& server, const NavWorldSettings& settings)
	: m_server(server), m_impl(std::make_unique<NavWorldImpl>()) {
	if (settings.maxAgents <= 0 || !(settings.maxAgentRadius > 0.0f)) {
		throw std::invalid_argument(
			"NavWorld: maxAgents and maxAgentRadius must be positive");
	}
	m_impl->maxAgents = settings.maxAgents;
	m_impl->maxAgentRadius = settings.maxAgentRadius;
	m_server._registerWorld(this);
}

NavWorld::~NavWorld() {
	for (NavAgent* agent : m_agents) {
		agent->_detachWorld();
	}
	m_agents.clear();
	m_impl.reset();
	m_server._unregisterWorld(this);
}

void NavWorld::setNavMesh(_ref<NavMesh> navMesh) {
	m_navMesh = std::move(navMesh);
	rebuildCrowd();
}

bool NavWorld::hasNavMesh() const {
	return m_navMesh != nullptr && m_navMesh->isValid();
}

NavPath NavWorld::findPath(const Vector3& start, const Vector3& end,
						   const Vector3& searchExtents) const {
	if (!hasNavMesh()) return NavPath();
	return m_navMesh->findPath(start, end, searchExtents);
}

std::optional<NavPoint> NavWorld::findNearestPoint(
	const Vector3& position, const Vector3& searchExtents) const {
	if (!hasNavMesh()) return std::nullopt;
	return m_navMesh->findNearestPoint(position, searchExtents);
}

void NavWorld::rebuildCrowd() {
	// Detach existing crowd indices before freeing the old crowd.
	for (NavAgent* agent : m_agents) {
		agent->_setCrowdIndex(-1);
	}
	if (m_impl->crowd) {
		dtFreeCrowd(m_impl->crowd);
		m_impl->crowd = nullptr;
	}

	if (!hasNavMesh()) return;

	m_impl->crowd = dtAllocCrowd();
	if (!m_impl->crowd) return;

	if (!m_impl->crowd->init(m_impl->maxAgents, m_impl->maxAgentRadius,
							 m_navMesh->_impl().navMesh)) {
		dtFreeCrowd(m_impl->crowd);
		m_impl->crowd = nullptr;
		return;
	}

	// Configure default query filter to walk all walkable polygons.
	if (dtQueryFilter* filter = m_impl->crowd->getEditableFilter(0)) {
		filter->setIncludeFlags(kNavPolyFlagWalk);
		filter->setExcludeFlags(0);
	}

	// Configure high-quality RVO obstacle avoidance profile (slot 3).
	dtObstacleAvoidanceParams oaParams{};
	std::memcpy(&oaParams, m_impl->crowd->getObstacleAvoidanceParams(0),
				sizeof(dtObstacleAvoidanceParams));
	oaParams.velBias = 0.4f;
	oaParams.weightDesVel = 2.0f;
	oaParams.weightCurVel = 0.75f;
	oaParams.weightSide = 0.75f;
	oaParams.weightToi = 2.5f;
	oaParams.horizTime = 2.5f;
	oaParams.gridSize = 33;
	oaParams.adaptiveDivs = 7;
	oaParams.adaptiveRings = 2;
	oaParams.adaptiveDepth = 5;
	m_impl->crowd->setObstacleAvoidanceParams(3, &oaParams);

	// Re-register all attached agents in the fresh crowd.
	for (NavAgent* agent : m_agents) {
		registerAgentInCrowd(agent);
	}
}

void NavWorld::registerAgentInCrowd(NavAgent* agent) {
	if (!m_impl->crowd || agent == nullptr || agent->world_object() == nullptr) return;

	const Vector3 pos = agent->world_object()->getWorldPosition();
	float p[3];
	toRecast(pos, p);

	dtCrowdAgentParams ap = makeCrowdParams(agent->getSettings());
	const int idx = m_impl->crowd->addAgent(p, &ap);
	agent->_setCrowdIndex(idx);
	agent->_setLastSyncedRevision(agent->world_object()->getTransformRevision());

	if (idx >= 0 && (agent->_hasPendingDestination() || agent->hasDestination())) {
		agent->_applyPendingDestination();
	}
}

void NavWorld::unregisterAgentFromCrowd(NavAgent* agent) {
	if (!m_impl->crowd || agent == nullptr) return;
	const int idx = agent->_crowdIndex();
	if (idx >= 0) {
		m_impl->crowd->removeAgent(idx);
		agent->_setCrowdIndex(-1);
	}
}

void NavWorld::_addAgent(NavAgent* agent) {
	if (agent == nullptr) return;
	m_agents.push_back(agent);
	if (m_impl->crowd) {
		registerAgentInCrowd(agent);
	}
}

void NavWorld::_removeAgent(NavAgent* agent) {
	if (agent == nullptr) return;
	unregisterAgentFromCrowd(agent);
	m_agents.erase(std::remove(m_agents.begin(), m_agents.end(), agent),
				   m_agents.end());
}

void NavWorld::step(float deltaTime) {
	if (!(deltaTime > 0.0f) || !m_impl->crowd || !hasNavMesh()) return;

	// 1. Pre-sync: detect external teleports via WorldObject transform revision.
	for (NavAgent* agent : m_agents) {
		WorldObject* obj = agent->world_object();
		if (obj == nullptr) continue;

		if (agent->_crowdIndex() < 0) {
			registerAgentInCrowd(agent);
			continue;
		}

		if (obj->getTransformRevision() != agent->_lastSyncedRevision()) {
			const bool hadDest = agent->hasDestination();
			const Vector3 dest = agent->getDestination();
			unregisterAgentFromCrowd(agent);
			registerAgentInCrowd(agent);
			if (hadDest) {
				agent->setDestination(dest);
			}
		}
	}

	// 2. Step Detour Crowd.
	m_impl->crowd->update(deltaTime, nullptr);

	// 3. Post-sync: write smoothed positions & rotations back to WorldObjects.
	for (NavAgent* agent : m_agents) {
		WorldObject* obj = agent->world_object();
		const int idx = agent->_crowdIndex();
		if (obj == nullptr || idx < 0) continue;

		const dtCrowdAgent* ag = m_impl->crowd->getAgent(idx);
		if (!ag || !ag->active) continue;

		const Vector3 newPos = fromRecast(ag->npos);
		const Vector3 vel = fromRecast(ag->vel);

		// Check arrival within stoppingDistance.
		if (agent->hasDestination()) {
			const Vector3 dest = agent->getDestination();
			const float dx = dest.x - newPos.x;
			const float dz = dest.z - newPos.z;
			const float horizDist = std::sqrt(dx * dx + dz * dz);
			const float vertDist = std::abs(dest.y - newPos.y);
			if (horizDist <= agent->getStoppingDistance() &&
				vertDist <= agent->getHeight()) {
				m_impl->crowd->resetMoveTarget(idx);
				if (dtCrowdAgent* editAg = m_impl->crowd->getEditableAgent(idx)) {
					editAg->vel[0] = editAg->vel[1] = editAg->vel[2] = 0.0f;
					editAg->dvel[0] = editAg->dvel[1] = editAg->dvel[2] = 0.0f;
				}
				agent->_markReachedDestination();
			}
		}

		obj->setWorldPosition(newPos);

		// Optional smooth yaw rotation facing movement velocity.
		if (agent->getUpdateRotation()) {
			const float horizSpeedSq = vel.x * vel.x + vel.z * vel.z;
			if (horizSpeedSq > 0.0025f) {
				const float targetYaw = std::atan2(vel.x, vel.z);
				const Quaternion targetRot =
					glm::angleAxis(targetYaw, Vector3(0.0f, 1.0f, 0.0f));
				const float t =
					std::clamp(agent->getRotationSpeed() * deltaTime, 0.0f, 1.0f);
				const Quaternion currentRot = obj->getWorldRotation();
				obj->setWorldRotation(glm::slerp(currentRot, targetRot, t));
			}
		}

		agent->_setLastSyncedRevision(obj->getTransformRevision());
	}
}

void NavWorld::debugDraw(DebugDrawList& out, const NavDebugSettings& settings) const {
	if (settings.drawNavMesh && hasNavMesh()) {
		m_navMesh->debugDraw(out, settings);
	}

	const float lift = settings.heightOffset;
	for (const NavAgent* agent : m_agents) {
		WorldObject* obj = const_cast<NavAgent*>(agent)->world_object();
		if (obj == nullptr) continue;

		const Vector3 pos = obj->getWorldPosition();

		if (settings.drawAgentBounds) {
			drawCylinderWireframe(out, pos, agent->getRadius(), agent->getHeight(),
								  settings.agentBoundsColor);
			const Vector3 vel = agent->getVelocity();
			if (glm::length(vel) > 0.05f) {
				const Vector3 mid = pos + Vector3(0.0f, agent->getHeight() * 0.5f, 0.0f);
				out.line(mid, mid + vel, settings.agentPathColor);
			}
		}

		if (settings.drawAgentPaths && agent->hasDestination()) {
			const NavPath path = agent->getCurrentPath();
			Vector3 prev = pos + Vector3(0.0f, lift + 0.1f, 0.0f);
			for (size_t i = 0; i < path.getPointCount(); ++i) {
				const Vector3 next =
					path.getPoint(i) + Vector3(0.0f, lift + 0.1f, 0.0f);
				out.line(prev, next, settings.agentPathColor);
				prev = next;
			}
			// Draw a small X marker at the destination.
			const Vector3 dst = agent->getDestination() + Vector3(0.0f, lift + 0.1f, 0.0f);
			constexpr float kCross = 0.25f;
			out.line(dst + Vector3(-kCross, 0.0f, -kCross),
					 dst + Vector3(kCross, 0.0f, kCross), settings.agentPathColor);
			out.line(dst + Vector3(-kCross, 0.0f, kCross),
					 dst + Vector3(kCross, 0.0f, -kCross), settings.agentPathColor);
		}
	}
}

}  // namespace bird
