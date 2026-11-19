#include "RigidBodyComponent.h"

#include <stdexcept>
#include <string>

#include "../scene/Scene.h"
#include "../scene/components/Script.h"
#include "PhysicsInternal.h"
#include "PhysicsWorld.h"

namespace bird {

namespace {

JPH::EMotionType toJoltMotion(MotionType type) {
	switch (type) {
		case MotionType::Static:
			return JPH::EMotionType::Static;
		case MotionType::Kinematic:
			return JPH::EMotionType::Kinematic;
		default:
			return JPH::EMotionType::Dynamic;
	}
}

void validate(const CollisionShape& shape, const RigidBodySettings& settings) {
	if (shape.isEmpty()) {
		throw std::invalid_argument("RigidBodyComponent: shape is empty");
	}
	if (shape.containsTriangleMesh() && settings.motionType == MotionType::Dynamic) {
		throw std::invalid_argument(
			"RigidBodyComponent: triangle meshes cannot be dynamic; use a convex hull");
	}
	if (settings.mass < 0.0f || settings.friction < 0.0f || settings.restitution < 0.0f ||
		settings.linearDamping < 0.0f || settings.angularDamping < 0.0f) {
		throw std::invalid_argument(
			"RigidBodyComponent: mass, friction, restitution and damping must be >= 0");
	}
}

/** The shape with a scale applied (made valid for shapes such as spheres). */
JPH::RefConst<JPH::Shape> scaled(const CollisionShape& shape, const Vector3& scale) {
	const JPH::Shape* base = nativeShape(shape);
	if (scale == Vector3(1.0f)) return base;
	if (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f) {
		throw std::invalid_argument("RigidBodyComponent: object scale must be non-zero");
	}
	return new JPH::ScaledShape(base, base->MakeScaleValid(toJolt(scale)));
}

JPH::ObjectLayer objectLayer(const RigidBodySettings& settings) {
	return layer_bits::pack(settings.layers, settings.mask,
							settings.motionType != MotionType::Static);
}

JPH::BodyInterface& bodyInterface(PhysicsWorld* world) {
	return world->_impl().system.GetBodyInterface();
}

}  // namespace

RigidBodyComponent::RigidBodyComponent(CollisionShape shape, const RigidBodySettings& settings)
	: m_shape(std::move(shape)), m_settings(settings) {
	validate(m_shape, m_settings);
}

RigidBodyComponent::~RigidBodyComponent() {
	destroyBody(false);	 // silent: partners may be mid-destruction too
}

PhysicsWorld* resolvePhysicsWorld(WorldObject* object, const char* who) {
	Scene* scene = object->getType() == SCENE ? static_cast<Scene*>(object) : object->getParent();
	if (scene == nullptr) {
		throw std::logic_error(std::string(who) + ": object is not in a scene");
	}
	return scene->getPhysicsWorld();
}

void RigidBodyComponent::_init() {
	m_world = resolvePhysicsWorld(world_object(), "RigidBodyComponent");
	createBody();
}

void RigidBodyComponent::_deinit() {
	destroyBody(true);
}

void RigidBodyComponent::createBody() {
	WorldObject* object = world_object();
	JPH::BodyCreationSettings settings(scaled(m_shape, object->getScale()),
									   bird::toJolt(object->getWorldPosition()),
									   bird::toJolt(object->getWorldRotation()),
									   toJoltMotion(m_settings.motionType), objectLayer(m_settings));
	settings.mFriction = m_settings.friction;
	settings.mRestitution = m_settings.restitution;
	settings.mLinearDamping = m_settings.linearDamping;
	settings.mAngularDamping = m_settings.angularDamping;
	settings.mGravityFactor = m_settings.gravityFactor;
	settings.mIsSensor = m_settings.isSensor;
	settings.mAllowSleeping = m_settings.allowSleeping;
	// Kinematic sensors should also notice static geometry.
	settings.mCollideKinematicVsNonDynamic = m_settings.isSensor;
	settings.mUserData = reinterpret_cast<JPH::uint64>(this);
	if (m_settings.motionType == MotionType::Dynamic && m_settings.mass > 0.0f) {
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		settings.mMassPropertiesOverride.mMass = m_settings.mass;
	} else if (m_settings.motionType == MotionType::Kinematic) {
		// Kinematic bodies ignore mass, but volumeless meshes cannot provide one.
		settings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
		settings.mMassPropertiesOverride.mMass = 1.0f;
		settings.mMassPropertiesOverride.mInertia = JPH::Mat44::sIdentity();
	}

	JPH::BodyInterface& bodies = bodyInterface(m_world);
	JPH::Body* body = bodies.CreateBody(settings);
	if (body == nullptr) {
		m_world = nullptr;
		throw std::runtime_error("RigidBodyComponent: world body limit reached");
	}
	bodies.AddBody(body->GetID(), m_settings.motionType == MotionType::Static
									  ? JPH::EActivation::DontActivate
									  : JPH::EActivation::Activate);
	m_bodyId = body->GetID().GetIndexAndSequenceNumber();
	m_syncedRevision = object->getTransformRevision();
	m_syncedScale = object->getScale();
	m_kinematicMoving = false;
	m_world->_addBody(this);
}

void RigidBodyComponent::destroyBody(bool notify) {
	if (m_world == nullptr || m_removing) return;
	m_removing = true;
	m_world->_removeBody(this, notify);  // ends contacts while both bodies resolve
	JPH::BodyInterface& bodies = bodyInterface(m_world);
	const JPH::BodyID id(m_bodyId);
	bodies.RemoveBody(id);
	bodies.DestroyBody(id);
	m_world = nullptr;
	m_bodyId = 0xFFFFFFFFu;
	m_removing = false;
}

void RigidBodyComponent::_forgetWorld() {
	m_world = nullptr;
	m_bodyId = 0xFFFFFFFFu;
}

void RigidBodyComponent::requireWorld(const char* what) const {
	if (m_world == nullptr) {
		throw std::logic_error(std::string("RigidBodyComponent::") + what +
							   ": component is not in a physics world");
	}
}

// ------------------------------------------------------------------- sync

void RigidBodyComponent::_preStep(float fixedDelta) {
	WorldObject* object = world_object();
	JPH::BodyInterface& bodies = m_world->_impl().system.GetBodyInterfaceNoLock();
	const JPH::BodyID id(m_bodyId);
	const uint64_t revision = object->getTransformRevision();

	if (revision == m_syncedRevision) {
		if (m_kinematicMoving) {
			// MoveKinematic left a velocity behind: the target was reached.
			bodies.SetLinearAndAngularVelocity(id, JPH::Vec3::sZero(), JPH::Vec3::sZero());
			m_kinematicMoving = false;
		}
		return;
	}

	const bool dynamic = m_settings.motionType == MotionType::Dynamic;
	if (object->getScale() != m_syncedScale) {
		bodies.SetShape(id, scaled(m_shape, object->getScale()), dynamic,
						JPH::EActivation::Activate);
		m_syncedScale = object->getScale();
	}
	const JPH::Vec3 position = bird::toJolt(object->getWorldPosition());
	const JPH::Quat rotation = bird::toJolt(object->getWorldRotation());
	if (m_settings.motionType == MotionType::Kinematic) {
		bodies.MoveKinematic(id, position, rotation, fixedDelta);
		m_kinematicMoving = true;
	} else {
		bodies.SetPositionAndRotation(
			id, position, rotation,
			dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
		if (m_settings.motionType == MotionType::Static) m_world->_staticGeometryChanged();
	}
	m_syncedRevision = revision;
}

void RigidBodyComponent::_writeBack(const Vector3& position, const Quaternion& rotation) {
	if (m_settings.motionType != MotionType::Dynamic) return;
	WorldObject* object = world_object();
	object->setWorldPosition(position);
	object->setWorldRotation(rotation);
	m_syncedRevision = object->getTransformRevision();	// not a game edit
}

std::vector<ContactCallbackFn> collectContactReceivers(WorldObject* object,
														 ContactEventKind kind,
														 const ContactCallbackFn& own) {
	void (ScriptComponent::*hook)(const ContactEvent&) = nullptr;
	switch (kind) {
		case ContactEventKind::ContactBegin:
			hook = &ScriptComponent::onContactBegin;
			break;
		case ContactEventKind::ContactEnd:
			hook = &ScriptComponent::onContactEnd;
			break;
		case ContactEventKind::TriggerEnter:
			hook = &ScriptComponent::onTriggerEnter;
			break;
		case ContactEventKind::TriggerExit:
			hook = &ScriptComponent::onTriggerExit;
			break;
	}
	std::vector<ContactCallbackFn> receivers;
	if (own) receivers.push_back(own);
	for (const auto& component : object->getComponents()) {
		if (auto* script = dynamic_cast<ScriptComponent*>(component.get())) {
			receivers.push_back([script, hook](const ContactEvent& e) { (script->*hook)(e); });
		}
	}
	return receivers;
}

std::vector<RigidBodyComponent::ContactCallback> RigidBodyComponent::_receivers(
	_EventKind kind) const {
	switch (kind) {
		case _EventKind::ContactBegin:
			return collectContactReceivers(m_object, kind, m_onContactBegin);
		case _EventKind::ContactEnd:
			return collectContactReceivers(m_object, kind, m_onContactEnd);
		case _EventKind::TriggerEnter:
			return collectContactReceivers(m_object, kind, m_onTriggerEnter);
		default:
			return collectContactReceivers(m_object, kind, m_onTriggerExit);
	}
}

// ---------------------------------------------------------------- runtime

void RigidBodyComponent::setShape(CollisionShape shape) {
	validate(shape, m_settings);
	m_shape = std::move(shape);
	if (m_world == nullptr) return;
	if (m_settings.motionType == MotionType::Static) m_world->_staticGeometryChanged();
	bodyInterface(m_world).SetShape(JPH::BodyID(m_bodyId),
									scaled(m_shape, world_object()->getScale()),
									m_settings.motionType == MotionType::Dynamic,
									JPH::EActivation::Activate);
}

Vector3 RigidBodyComponent::getLinearVelocity() const {
	requireWorld("getLinearVelocity");
	return toBird(bodyInterface(m_world).GetLinearVelocity(JPH::BodyID(m_bodyId)));
}

void RigidBodyComponent::setLinearVelocity(const Vector3& velocity) {
	requireWorld("setLinearVelocity");
	bodyInterface(m_world).SetLinearVelocity(JPH::BodyID(m_bodyId), bird::toJolt(velocity));
}

Vector3 RigidBodyComponent::getAngularVelocity() const {
	requireWorld("getAngularVelocity");
	return toBird(bodyInterface(m_world).GetAngularVelocity(JPH::BodyID(m_bodyId)));
}

void RigidBodyComponent::setAngularVelocity(const Vector3& velocity) {
	requireWorld("setAngularVelocity");
	bodyInterface(m_world).SetAngularVelocity(JPH::BodyID(m_bodyId), bird::toJolt(velocity));
}

void RigidBodyComponent::applyForce(const Vector3& force) {
	requireWorld("applyForce");
	bodyInterface(m_world).AddForce(JPH::BodyID(m_bodyId), bird::toJolt(force));
}

void RigidBodyComponent::applyForceAt(const Vector3& force, const Vector3& point) {
	requireWorld("applyForceAt");
	bodyInterface(m_world).AddForce(JPH::BodyID(m_bodyId), bird::toJolt(force),
									bird::toJolt(point));
}

void RigidBodyComponent::applyTorque(const Vector3& torque) {
	requireWorld("applyTorque");
	bodyInterface(m_world).AddTorque(JPH::BodyID(m_bodyId), bird::toJolt(torque));
}

void RigidBodyComponent::applyImpulse(const Vector3& impulse) {
	requireWorld("applyImpulse");
	bodyInterface(m_world).AddImpulse(JPH::BodyID(m_bodyId), bird::toJolt(impulse));
}

void RigidBodyComponent::applyImpulseAt(const Vector3& impulse, const Vector3& point) {
	requireWorld("applyImpulseAt");
	bodyInterface(m_world).AddImpulse(JPH::BodyID(m_bodyId), bird::toJolt(impulse),
									  bird::toJolt(point));
}

void RigidBodyComponent::teleport(const Vector3& position, const Quaternion& rotation) {
	requireWorld("teleport");
	WorldObject* object = world_object();
	object->setWorldPosition(position);
	object->setWorldRotation(rotation);
	bodyInterface(m_world).SetPositionAndRotation(
		JPH::BodyID(m_bodyId), bird::toJolt(position), bird::toJolt(rotation),
		m_settings.motionType == MotionType::Static ? JPH::EActivation::DontActivate
													: JPH::EActivation::Activate);
	m_syncedRevision = object->getTransformRevision();
	if (m_settings.motionType == MotionType::Static) m_world->_staticGeometryChanged();
}

void RigidBodyComponent::wake() {
	requireWorld("wake");
	bodyInterface(m_world).ActivateBody(JPH::BodyID(m_bodyId));
}

bool RigidBodyComponent::isSleeping() const {
	requireWorld("isSleeping");
	return !bodyInterface(m_world).IsActive(JPH::BodyID(m_bodyId));
}

float RigidBodyComponent::getMass() const {
	requireWorld("getMass");
	if (m_settings.motionType != MotionType::Dynamic) return 0.0f;
	JPH::BodyLockRead lock(m_world->_impl().system.GetBodyLockInterface(),
						   JPH::BodyID(m_bodyId));
	if (!lock.Succeeded()) return 0.0f;
	const float inverseMass = lock.GetBody().GetMotionProperties()->GetInverseMass();
	return inverseMass > 0.0f ? 1.0f / inverseMass : 0.0f;
}

void RigidBodyComponent::setFriction(float friction) {
	if (friction < 0.0f) throw std::invalid_argument("RigidBodyComponent: friction < 0");
	m_settings.friction = friction;
	if (m_world) bodyInterface(m_world).SetFriction(JPH::BodyID(m_bodyId), friction);
}

void RigidBodyComponent::setRestitution(float restitution) {
	if (restitution < 0.0f) throw std::invalid_argument("RigidBodyComponent: restitution < 0");
	m_settings.restitution = restitution;
	if (m_world) bodyInterface(m_world).SetRestitution(JPH::BodyID(m_bodyId), restitution);
}

void RigidBodyComponent::setGravityFactor(float factor) {
	m_settings.gravityFactor = factor;
	if (m_world) bodyInterface(m_world).SetGravityFactor(JPH::BodyID(m_bodyId), factor);
}

void RigidBodyComponent::setLayers(PhysicsLayerMask layers, PhysicsLayerMask mask) {
	m_settings.layers = layers;
	m_settings.mask = mask;
	if (m_world) bodyInterface(m_world).SetObjectLayer(JPH::BodyID(m_bodyId), objectLayer(m_settings));
}

RigidBodyComponent& RigidBodyComponent::setOnContactBegin(ContactCallback callback) {
	m_onContactBegin = std::move(callback);
	return *this;
}

RigidBodyComponent& RigidBodyComponent::setOnContactEnd(ContactCallback callback) {
	m_onContactEnd = std::move(callback);
	return *this;
}

RigidBodyComponent& RigidBodyComponent::setOnTriggerEnter(ContactCallback callback) {
	m_onTriggerEnter = std::move(callback);
	return *this;
}

RigidBodyComponent& RigidBodyComponent::setOnTriggerExit(ContactCallback callback) {
	m_onTriggerExit = std::move(callback);
	return *this;
}

}  // namespace bird
