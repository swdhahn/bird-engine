#include "PhysicsWorld.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <unordered_set>

#include "PhysicsInternal.h"
#include "PhysicsServer.h"
#include "CharacterControllerComponent.h"
#include "RigidBodyComponent.h"

namespace bird {

// ------------------------------------------------------------ contact queue

void ContactQueue::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
								  const JPH::ContactManifold& manifold,
								  JPH::ContactSettings& settings) {
	RawContact contact{true,
					   body1.IsSensor() || body2.IsSensor(),
					   body1.GetID(),
					   body2.GetID(),
					   toBird(JPH::Vec3(manifold.GetWorldSpaceContactPointOn1(0))),
					   toBird(manifold.mWorldSpaceNormal)};
	std::lock_guard<std::mutex> lock(m_mutex);
	m_events.push_back(contact);
}

void ContactQueue::OnContactRemoved(const JPH::SubShapeIDPair& pair) {
	RawContact contact{false, false, pair.GetBody1ID(), pair.GetBody2ID(), Vector3(0.0f),
					   Vector3(0.0f)};
	std::lock_guard<std::mutex> lock(m_mutex);
	m_events.push_back(contact);
}

std::vector<RawContact> ContactQueue::drain() {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::vector<RawContact> events;
	events.swap(m_events);
	return events;
}

// ------------------------------------------------------------------ helpers

namespace {

using EventKind = RigidBodyComponent::_EventKind;

std::pair<uint32_t, uint32_t> pairKey(uint32_t a, uint32_t b) {
	return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

RigidBodyComponent* lookup(const PhysicsWorldImpl& impl, uint32_t id) {
	auto it = impl.byId.find(id);
	return it == impl.byId.end() ? nullptr : it->second;
}

/**
 * Delivers one event to every receiver of `selfId`, stopping as soon as a
 * receiver destroys either body (the pointers would dangle).
 */
void deliver(const PhysicsWorldImpl& impl, uint32_t selfId, uint32_t otherId,
			 EventKind kind, const Vector3& point, const Vector3& normal) {
	RigidBodyComponent* self = lookup(impl, selfId);
	RigidBodyComponent* other = lookup(impl, otherId);
	if (self == nullptr || other == nullptr) return;
	for (const auto& receiver : self->_receivers(kind)) {
		if (lookup(impl, selfId) != self || lookup(impl, otherId) != other) return;
		receiver(ContactEvent{self, other, point, normal});
	}
}

void deliverPair(const PhysicsWorldImpl& impl, uint32_t a, uint32_t b, EventKind kind,
				 const Vector3& point, const Vector3& normalAtoB) {
	deliver(impl, a, b, kind, point, normalAtoB);
	deliver(impl, b, a, kind, point, -normalAtoB);
}

/** The Jolt body a query should skip (invalid when none). */
JPH::BodyID ignoredBody(const Component* ignore) {
	if (auto* body = dynamic_cast<const RigidBodyComponent*>(ignore)) {
		if (body->isInWorld()) return JPH::BodyID(body->_bodyId());
	}
	if (auto* character = dynamic_cast<const CharacterControllerComponent*>(ignore)) {
		if (character->isInWorld()) return JPH::BodyID(character->_innerBodyId());
	}
	return JPH::BodyID();
}

/** Fills a hit's body and object from a Jolt body ID. */
template <typename Hit>
void identify(const PhysicsWorldImpl& impl, uint32_t id, Hit& hit) {
	hit.body = lookup(impl, id);
	if (hit.body != nullptr) {
		hit.object = hit.body->world_object();
		return;
	}
	auto it = impl.characterByInnerBody.find(id);
	if (it != impl.characterByInnerBody.end()) hit.object = it->second->world_object();
}

Vector3 normalizedDirection(const Vector3& direction, const char* what) {
	const float length = glm::length(direction);
	if (!(length > 0.0f)) {
		throw std::invalid_argument(std::string("PhysicsWorld::") + what +
									": direction must be non-zero");
	}
	return direction / length;
}

void requireShape(const CollisionShape& shape, const char* what) {
	if (shape.isEmpty()) {
		throw std::invalid_argument(std::string("PhysicsWorld::") + what + ": empty shape");
	}
}

/** Centre-of-mass transform for a shape placed at position/rotation. */
JPH::RMat44 comTransform(const JPH::Shape* shape, const Vector3& position,
						 const Quaternion& rotation) {
	return JPH::RMat44::sRotationTranslation(toJolt(rotation), toJolt(position))
		.PreTranslated(shape->GetCenterOfMass());
}

}  // namespace

// -------------------------------------------------------------------- world

PhysicsWorld::PhysicsWorld(PhysicsServer& server, const PhysicsWorldSettings& settings)
	: m_server(server), m_impl(std::make_unique<PhysicsWorldImpl>()) {
	if (settings.maxBodies == 0 || settings.maxBodyPairs == 0 ||
		settings.maxContactConstraints == 0) {
		throw std::invalid_argument("PhysicsWorld: capacities must be positive");
	}
	PhysicsServerImpl& serverImpl = server._impl();
	m_impl->server = &serverImpl;
	m_impl->system.Init(settings.maxBodies, 0, settings.maxBodyPairs,
						settings.maxContactConstraints, serverImpl.broadPhaseLayers,
						serverImpl.objectVsBroadPhase, serverImpl.objectPairs);
	m_impl->system.SetGravity(toJolt(settings.gravity));
	m_impl->system.SetContactListener(&m_impl->contacts);
	server._registerWorld(this);
}

PhysicsWorld::~PhysicsWorld() {
	// Bodies are freed with the PhysicsSystem; components just let go.
	for (RigidBodyComponent* body : m_impl->bodies) body->_forgetWorld();
	// Characters release their CharacterVirtual (and inner body) now, while
	// the PhysicsSystem still exists.
	for (CharacterControllerComponent* character : std::vector(m_impl->characters)) {
		character->_forgetWorld();
	}
	m_server._unregisterWorld(this);
}

void PhysicsWorld::step(float fixedDelta) {
	if (!(fixedDelta > 0.0f)) {
		throw std::invalid_argument("PhysicsWorld::step: fixedDelta must be positive");
	}
	if (m_stepping) {
		throw std::logic_error("PhysicsWorld::step: already stepping");
	}
	m_stepping = true;
	PhysicsWorldImpl& impl = *m_impl;

	for (RigidBodyComponent* body : impl.bodies) body->_preStep(fixedDelta);
	for (CharacterControllerComponent* character : impl.characters) character->_step(fixedDelta);

	const JPH::EPhysicsUpdateError error =
		impl.system.Update(fixedDelta, 1, impl.server->tempAllocator.get(),
						   impl.server->jobSystem.get());
	if (error != JPH::EPhysicsUpdateError::None) {
		std::cerr << "PhysicsWorld::step: Jolt capacity exceeded (error "
				  << uint32_t(error) << "); raise PhysicsWorldSettings limits." << std::endl;
	}

	impl.activeScratch.clear();
	impl.system.GetActiveBodies(JPH::EBodyType::RigidBody, impl.activeScratch);
	const JPH::BodyInterface& bodies = impl.system.GetBodyInterfaceNoLock();
	for (const JPH::BodyID& id : impl.activeScratch) {
		RigidBodyComponent* body = lookup(impl, id.GetIndexAndSequenceNumber());
		if (body == nullptr || body->getMotionType() != MotionType::Dynamic) continue;
		JPH::RVec3 position;
		JPH::Quat rotation;
		bodies.GetPositionAndRotation(id, position, rotation);
		body->_writeBack(toBird(JPH::Vec3(position)), toBird(rotation));
	}

	m_stepping = false;
	dispatchContacts();
}

void PhysicsWorld::dispatchContacts() {
	PhysicsWorldImpl& impl = *m_impl;
	const JPH::BodyInterface& bodies = impl.system.GetBodyInterfaceNoLock();
	// A moving body that is no longer active fell asleep.
	auto asleep = [&](uint32_t id) {
		RigidBodyComponent* body = lookup(impl, id);
		return body != nullptr && body->getMotionType() != MotionType::Static &&
			   !bodies.IsActive(JPH::BodyID(id));
	};

	for (const RawContact& contact : impl.contacts.drain()) {
		const uint32_t a = contact.body1.GetIndexAndSequenceNumber();
		const uint32_t b = contact.body2.GetIndexAndSequenceNumber();
		const auto key = pairKey(a, b);
		if (contact.begin) {
			// Skip pairs whose bodies were destroyed after Jolt recorded this.
			if (lookup(impl, a) == nullptr || lookup(impl, b) == nullptr) continue;
			PhysicsWorldImpl::PairState& state = impl.pairs[key];
			state.sensor = contact.sensor;
			const bool resumed = state.dormant;	 // woke up still touching
			state.dormant = false;
			if (++state.touching != 1 || resumed) continue;
			deliverPair(impl, a, b, contact.sensor ? EventKind::TriggerEnter : EventKind::ContactBegin,
						contact.point, contact.normal);
		} else {
			auto it = impl.pairs.find(key);
			if (it == impl.pairs.end() || it->second.dormant) continue;
			if (--it->second.touching > 0) continue;
			if (asleep(a) || asleep(b)) {
				it->second.dormant = true;
				continue;
			}
			const bool sensor = it->second.sensor;
			impl.pairs.erase(it);
			deliverPair(impl, a, b, sensor ? EventKind::TriggerExit : EventKind::ContactEnd,
						Vector3(0.0f), Vector3(0.0f));
		}
	}

	// Dormant pairs whose bodies woke without re-detecting the contact have
	// separated (for example, one was teleported away).
	std::vector<std::pair<std::pair<uint32_t, uint32_t>, bool>> separated;
	for (auto it = impl.pairs.begin(); it != impl.pairs.end();) {
		const auto [a, b] = it->first;
		if (it->second.dormant && !asleep(a) && !asleep(b)) {
			separated.emplace_back(it->first, it->second.sensor);
			it = impl.pairs.erase(it);
		} else {
			++it;
		}
	}
	for (const auto& [key, sensor] : separated) {
		deliverPair(impl, key.first, key.second,
					sensor ? EventKind::TriggerExit : EventKind::ContactEnd, Vector3(0.0f),
					Vector3(0.0f));
	}
}

void PhysicsWorld::_addBody(RigidBodyComponent* body) {
	body->_setIndex(m_impl->bodies.size());
	m_impl->bodies.push_back(body);
	m_impl->byId[body->_bodyId()] = body;
}

void PhysicsWorld::_removeBody(RigidBodyComponent* body) {
	PhysicsWorldImpl& impl = *m_impl;
	const uint32_t id = body->_bodyId();

	// End every pair this body is part of, while both bodies still resolve.
	std::vector<std::pair<uint32_t, bool>> partners;
	for (auto it = impl.pairs.begin(); it != impl.pairs.end();) {
		if (it->first.first == id || it->first.second == id) {
			const uint32_t partner = it->first.first == id ? it->first.second : it->first.first;
			partners.emplace_back(partner, it->second.sensor);
			it = impl.pairs.erase(it);
		} else {
			++it;
		}
	}
	for (const auto& [partner, sensor] : partners) {
		deliver(impl, partner, id, sensor ? EventKind::TriggerExit : EventKind::ContactEnd,
				Vector3(0.0f), Vector3(0.0f));
	}

	// Swap-remove: O(1).
	const size_t index = body->_index();
	if (index < impl.bodies.size() && impl.bodies[index] == body) {
		impl.bodies[index] = impl.bodies.back();
		impl.bodies[index]->_setIndex(index);
		impl.bodies.pop_back();
	}
	impl.byId.erase(id);
}

void PhysicsWorld::setGravity(const Vector3& gravity) {
	m_impl->system.SetGravity(toJolt(gravity));
}

Vector3 PhysicsWorld::getGravity() const {
	return toBird(m_impl->system.GetGravity());
}

size_t PhysicsWorld::getBodyCount() const {
	return m_impl->bodies.size();
}

size_t PhysicsWorld::getCharacterCount() const {
	return m_impl->characters.size();
}

size_t PhysicsWorld::getActiveBodyCount() const {
	return m_impl->system.GetNumActiveBodies(JPH::EBodyType::RigidBody);
}

// ------------------------------------------------------------------ queries

std::optional<RaycastHit> PhysicsWorld::raycast(const Vector3& origin, const Vector3& direction,
												float maxDistance, PhysicsLayerMask mask,
												const Component* ignore) const {
	const Vector3 dir = normalizedDirection(direction, "raycast");
	if (!(maxDistance > 0.0f)) {
		throw std::invalid_argument("PhysicsWorld::raycast: maxDistance must be positive");
	}
	const JPH::RRayCast ray{toJolt(origin), toJolt(dir * maxDistance)};
	JPH::RayCastResult result;
	const JPH::IgnoreSingleBodyFilter ignoreFilter(ignoredBody(ignore));
	const QueryLayerFilter layerFilter(mask);
	if (!m_impl->system.GetNarrowPhaseQuery().CastRay(
			ray, result, JPH::BroadPhaseLayerFilter(), layerFilter,
			ignoreFilter)) {
		return std::nullopt;
	}

	RaycastHit hit;
	identify(*m_impl, result.mBodyID.GetIndexAndSequenceNumber(), hit);
	hit.distance = result.mFraction * maxDistance;
	hit.point = origin + dir * hit.distance;
	JPH::BodyLockRead lock(m_impl->system.GetBodyLockInterface(), result.mBodyID);
	if (lock.Succeeded()) {
		JPH::Vec3 normal =
			lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, toJolt(hit.point));
		hit.normal = toBird(normal);
		if (glm::dot(hit.normal, dir) > 0.0f) hit.normal = -hit.normal;	 // back face
	}
	return hit;
}

std::vector<RaycastHit> PhysicsWorld::raycastAll(const Vector3& origin,
												 const Vector3& direction, float maxDistance,
												 PhysicsLayerMask mask,
												 const Component* ignore) const {
	const Vector3 dir = normalizedDirection(direction, "raycastAll");
	if (!(maxDistance > 0.0f)) {
		throw std::invalid_argument("PhysicsWorld::raycastAll: maxDistance must be positive");
	}
	const JPH::RRayCast ray{toJolt(origin), toJolt(dir * maxDistance)};
	JPH::RayCastSettings settings;
	JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
	const JPH::IgnoreSingleBodyFilter ignoreFilter(ignoredBody(ignore));
	const QueryLayerFilter layerFilter(mask);
	m_impl->system.GetNarrowPhaseQuery().CastRay(ray, settings, collector,
												 JPH::BroadPhaseLayerFilter(), layerFilter,
												 ignoreFilter);
	collector.Sort();

	std::vector<RaycastHit> hits;
	std::unordered_set<uint32_t> seen;
	for (const JPH::RayCastResult& result : collector.mHits) {
		const uint32_t id = result.mBodyID.GetIndexAndSequenceNumber();
		if (!seen.insert(id).second) continue;	// nearest hit per body only
		RaycastHit hit;
		identify(*m_impl, id, hit);
		hit.distance = result.mFraction * maxDistance;
		hit.point = origin + dir * hit.distance;
		JPH::BodyLockRead lock(m_impl->system.GetBodyLockInterface(), result.mBodyID);
		if (lock.Succeeded()) {
			hit.normal = toBird(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2,
																		  toJolt(hit.point)));
			if (glm::dot(hit.normal, dir) > 0.0f) hit.normal = -hit.normal;
		}
		hits.push_back(hit);
	}
	return hits;
}

std::optional<ShapeCastHit> PhysicsWorld::shapeCast(const CollisionShape& shape,
													const Vector3& from,
													const Quaternion& rotation,
													const Vector3& to, PhysicsLayerMask mask,
													const Component* ignore) const {
	requireShape(shape, "shapeCast");
	const JPH::Shape* native = nativeShape(shape);
	const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
		native, JPH::Vec3::sReplicate(1.0f),
		JPH::RMat44::sRotationTranslation(toJolt(rotation), toJolt(from)), toJolt(to - from));
	JPH::ShapeCastSettings settings;
	JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
	const JPH::IgnoreSingleBodyFilter ignoreFilter(ignoredBody(ignore));
	const QueryLayerFilter layerFilter(mask);
	m_impl->system.GetNarrowPhaseQuery().CastShape(
		cast, settings, JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(),
		layerFilter, ignoreFilter);
	if (!collector.HadHit()) return std::nullopt;

	const JPH::ShapeCastResult& result = collector.mHit;
	ShapeCastHit hit;
	identify(*m_impl, result.mBodyID2.GetIndexAndSequenceNumber(), hit);
	hit.fraction = result.mFraction;
	hit.point = toBird(JPH::Vec3(result.mContactPointOn2));
	const JPH::Vec3 axis = result.mPenetrationAxis;
	hit.normal = axis.IsNearZero() ? Vector3(0.0f) : toBird(-axis.Normalized());
	return hit;
}

std::vector<RigidBodyComponent*> PhysicsWorld::overlap(const CollisionShape& shape,
													   const Vector3& position,
													   const Quaternion& rotation,
													   PhysicsLayerMask mask,
													   const Component* ignore) const {
	requireShape(shape, "overlap");
	const JPH::Shape* native = nativeShape(shape);
	JPH::CollideShapeSettings settings;
	JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
	const JPH::IgnoreSingleBodyFilter ignoreFilter(ignoredBody(ignore));
	const QueryLayerFilter layerFilter(mask);
	m_impl->system.GetNarrowPhaseQuery().CollideShape(
		native, JPH::Vec3::sReplicate(1.0f), comTransform(native, position, rotation),
		settings, JPH::RVec3::sZero(), collector, JPH::BroadPhaseLayerFilter(), layerFilter,
		ignoreFilter);

	std::vector<RigidBodyComponent*> bodies;
	std::unordered_set<uint32_t> seen;
	for (const JPH::CollideShapeResult& result : collector.mHits) {
		const uint32_t id = result.mBodyID2.GetIndexAndSequenceNumber();
		if (!seen.insert(id).second) continue;
		if (RigidBodyComponent* body = lookup(*m_impl, id)) bodies.push_back(body);
	}
	return bodies;
}

}  // namespace bird
