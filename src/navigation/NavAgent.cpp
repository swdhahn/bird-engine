#include "NavAgent.h"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "NavInternal.h"
#include "NavWorld.h"

namespace bird {

namespace {

void validateAgentSettings(const NavAgentSettings& s) {
	if (!(s.radius > 0.0f) || !(s.height > 0.0f)) {
		throw std::invalid_argument(
			"NavAgent: radius and height must be positive");
	}
	if (!(s.maxSpeed > 0.0f) || !(s.maxAcceleration > 0.0f)) {
		throw std::invalid_argument(
			"NavAgent: maxSpeed and maxAcceleration must be positive");
	}
	if (s.stoppingDistance < 0.0f) {
		throw std::invalid_argument(
			"NavAgent: stoppingDistance must be non-negative");
	}
	if (s.rotationSpeed < 0.0f) {
		throw std::invalid_argument(
			"NavAgent: rotationSpeed must be non-negative");
	}
}

}  // namespace

NavAgent::NavAgent(const NavAgentSettings& settings) : m_settings(settings) {
	validateAgentSettings(settings);
}

NavAgent::~NavAgent() {
	if (m_world != nullptr) {
		m_world->_removeAgent(this);
		m_world = nullptr;
	}
}

void NavAgent::_init() {
	WorldObject* obj = world_object();
	if (obj == nullptr) return;
	Scene* parent = obj->getParent();
	if (parent == nullptr) return;

	m_world = parent->getNavWorld();
	m_lastSyncedPosition = obj->getWorldPosition();
	if (m_world != nullptr) {
		m_world->_addAgent(this);
	}
}

void NavAgent::_deinit() {
	if (m_world != nullptr) {
		m_world->_removeAgent(this);
		m_world = nullptr;
	}
	m_crowdIndex = -1;
}

void NavAgent::_detachWorld() {
	m_world = nullptr;
	m_crowdIndex = -1;
}

bool NavAgent::setDestination(const Vector3& target) {
	m_destination = target;
	m_reachedDestination = false;

	if (m_world == nullptr || !m_world->hasNavMesh() || m_crowdIndex < 0) {
		// Queue until the agent joins a crowd with a valid NavMesh.
		m_hasDestination = true;
		m_pendingDestination = true;
		return true;
	}

	dtCrowd* crowd = m_world->_impl().crowd;
	if (!crowd) {
		m_hasDestination = true;
		m_pendingDestination = true;
		return true;
	}

	const dtNavMeshQuery* navQuery = crowd->getNavMeshQuery();
	const dtQueryFilter* filter = crowd->getFilter(0);
	const float* halfExtents = crowd->getQueryExtents();

	float targetPos[3];
	toRecast(target, targetPos);
	dtPolyRef targetRef = 0;
	float nearestPos[3] = {};

	if (dtStatusFailed(navQuery->findNearestPoly(targetPos, halfExtents, filter,
												 &targetRef, nearestPos)) ||
		targetRef == 0) {
		m_hasDestination = false;
		m_pendingDestination = false;
		return false;
	}

	if (!crowd->requestMoveTarget(m_crowdIndex, targetRef, nearestPos)) {
		m_hasDestination = false;
		m_pendingDestination = false;
		return false;
	}

	m_destination = fromRecast(nearestPos);
	m_hasDestination = true;
	m_pendingDestination = false;
	return true;
}

void NavAgent::_applyPendingDestination() {
	if (!m_pendingDestination && !m_hasDestination) return;
	const Vector3 target = m_destination;
	m_pendingDestination = false;
	setDestination(target);
}

void NavAgent::_markReachedDestination() {
	m_hasDestination = false;
	m_pendingDestination = false;
	m_reachedDestination = true;
}

void NavAgent::stop() {
	m_hasDestination = false;
	m_pendingDestination = false;
	m_reachedDestination = false;

	if (m_world != nullptr && m_world->_impl().crowd != nullptr && m_crowdIndex >= 0) {
		dtCrowd* crowd = m_world->_impl().crowd;
		crowd->resetMoveTarget(m_crowdIndex);
		if (dtCrowdAgent* ag = crowd->getEditableAgent(m_crowdIndex)) {
			ag->vel[0] = ag->vel[1] = ag->vel[2] = 0.0f;
			ag->dvel[0] = ag->dvel[1] = ag->dvel[2] = 0.0f;
		}
	}
}

bool NavAgent::isMoving() const {
	const Vector3 vel = getVelocity();
	return (vel.x * vel.x + vel.y * vel.y + vel.z * vel.z) > 0.0025f;
}

Vector3 NavAgent::getVelocity() const {
	if (m_world == nullptr || m_world->_impl().crowd == nullptr || m_crowdIndex < 0) {
		return Vector3(0.0f);
	}
	const dtCrowdAgent* ag = m_world->_impl().crowd->getAgent(m_crowdIndex);
	if (!ag || !ag->active) return Vector3(0.0f);
	return fromRecast(ag->vel);
}

float NavAgent::getRemainingDistance() const {
	if (!m_hasDestination || m_object == nullptr) return 0.0f;
	return glm::length(m_destination - m_object->getWorldPosition());
}

NavPath NavAgent::getCurrentPath() const {
	if (!m_hasDestination || m_world == nullptr || m_world->_impl().crowd == nullptr ||
		m_crowdIndex < 0) {
		return NavPath();
	}
	const dtCrowdAgent* ag = m_world->_impl().crowd->getAgent(m_crowdIndex);
	if (!ag || !ag->active) return NavPath();

	std::vector<Vector3> corners;
	corners.reserve(static_cast<size_t>(ag->ncorners) + 1);
	for (int i = 0; i < ag->ncorners; ++i) {
		corners.emplace_back(ag->cornerVerts[i * 3 + 0],
							 ag->cornerVerts[i * 3 + 1],
							 ag->cornerVerts[i * 3 + 2]);
	}
	// If Detour has not yet populated corners on the first frame, include the target.
	if (corners.empty()) {
		corners.push_back(m_destination);
	}
	return NavPath(std::move(corners), false);
}

void NavAgent::pushParamsToCrowd() {
	if (m_world == nullptr || m_world->_impl().crowd == nullptr || m_crowdIndex < 0) {
		return;
	}
	dtCrowdAgentParams ap{};
	ap.radius = m_settings.radius;
	ap.height = m_settings.height;
	ap.maxAcceleration = m_settings.maxAcceleration;
	ap.maxSpeed = m_settings.maxSpeed;
	ap.collisionQueryRange = m_settings.radius * 12.0f;
	ap.pathOptimizationRange = m_settings.radius * 30.0f;
	ap.separationWeight = m_settings.separationWeight;
	ap.updateFlags = DT_CROWD_ANTICIPATE_TURNS | DT_CROWD_OPTIMIZE_VIS |
					 DT_CROWD_OPTIMIZE_TOPO | DT_CROWD_SEPARATION;
	if (m_settings.obstacleAvoidance) {
		ap.updateFlags |= DT_CROWD_OBSTACLE_AVOIDANCE;
	}
	ap.obstacleAvoidanceType = 3;
	ap.queryFilterType = 0;
	ap.userData = nullptr;
	m_world->_impl().crowd->updateAgentParameters(m_crowdIndex, &ap);
}

void NavAgent::setMaxSpeed(float speed) {
	if (!(speed > 0.0f)) {
		throw std::invalid_argument("NavAgent::setMaxSpeed: speed must be positive");
	}
	m_settings.maxSpeed = speed;
	pushParamsToCrowd();
}

void NavAgent::setMaxAcceleration(float acceleration) {
	if (!(acceleration > 0.0f)) {
		throw std::invalid_argument(
			"NavAgent::setMaxAcceleration: acceleration must be positive");
	}
	m_settings.maxAcceleration = acceleration;
	pushParamsToCrowd();
}

void NavAgent::setStoppingDistance(float distance) {
	if (distance < 0.0f) {
		throw std::invalid_argument(
			"NavAgent::setStoppingDistance: distance must be non-negative");
	}
	m_settings.stoppingDistance = distance;
}

void NavAgent::setRadius(float radius) {
	if (!(radius > 0.0f)) {
		throw std::invalid_argument("NavAgent::setRadius: radius must be positive");
	}
	m_settings.radius = radius;
	pushParamsToCrowd();
}

void NavAgent::setHeight(float height) {
	if (!(height > 0.0f)) {
		throw std::invalid_argument("NavAgent::setHeight: height must be positive");
	}
	m_settings.height = height;
	pushParamsToCrowd();
}

void NavAgent::setRotationSpeed(float speed) {
	if (speed < 0.0f) {
		throw std::invalid_argument(
			"NavAgent::setRotationSpeed: speed must be non-negative");
	}
	m_settings.rotationSpeed = speed;
}

}  // namespace bird
