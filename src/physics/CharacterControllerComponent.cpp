#include "CharacterControllerComponent.h"

#include <stdexcept>
#include <string>
#include <unordered_set>

#include "PhysicsInternal.h"
#include "PhysicsWorld.h"
#include "RigidBodyComponent.h"

namespace bird {

namespace {

void validate(const CharacterSettings& s) {
	if (s.shape.isEmpty()) {
		if (!(s.radius > 0.0f) || !(s.height > 2.0f * s.radius)) {
			throw std::invalid_argument(
				"CharacterControllerComponent: needs radius > 0 and height > 2 * radius");
		}
	} else if (s.shape.containsTriangleMesh()) {
		throw std::invalid_argument(
			"CharacterControllerComponent: a character shape cannot be a triangle mesh");
	}
	if (s.mass <= 0.0f || s.pushStrength < 0.0f || s.stepHeight < 0.0f ||
		s.stickToFloorDistance < 0.0f || s.maxSlopeDegrees <= 0.0f || s.maxSlopeDegrees >= 90.0f) {
		throw std::invalid_argument(
			"CharacterControllerComponent: mass must be positive, strength and distances "
			"non-negative, slope in (0, 90) degrees");
	}
}

/** The default upright capsule, lifted so the object's position is the feet. */
JPH::RefConst<JPH::Shape> characterShape(const CharacterSettings& s) {
	if (!s.shape.isEmpty()) return nativeShape(s.shape);
	const float halfCylinder = 0.5f * s.height - s.radius;
	return JPH::RotatedTranslatedShapeSettings(JPH::Vec3(0, 0.5f * s.height, 0),
											   JPH::Quat::sIdentity(),
											   new JPH::CapsuleShape(halfCylinder, s.radius))
		.Create()
		.Get();
}

JPH::Vec3 gravityFor(PhysicsWorld* world, float factor) {
	return world->_impl().system.GetGravity() * factor;
}

}  // namespace

CharacterControllerComponent::CharacterControllerComponent(const CharacterSettings& settings)
	: m_settings(settings) {
	validate(m_settings);
}

CharacterControllerComponent::~CharacterControllerComponent() {
	destroyCharacter(false);  // silent: partners may be mid-destruction too
}

void CharacterControllerComponent::_init() {
	m_world = resolvePhysicsWorld(world_object(), "CharacterControllerComponent");
	PhysicsWorldImpl& impl = m_world->_impl();
	WorldObject* object = world_object();

	JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
	JPH::RefConst<JPH::Shape> shape = characterShape(m_settings);
	settings->mShape = shape;
	settings->mInnerBodyShape = shape;	// lets bodies and queries see the character
	settings->mMaxSlopeAngle = JPH::DegreesToRadians(m_settings.maxSlopeDegrees);
	settings->mMass = m_settings.mass;
	settings->mMaxStrength = m_settings.pushStrength;
	if (m_settings.shape.isEmpty()) {
		// Contacts below the centre of the lower hemisphere support the character.
		settings->mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -m_settings.radius);
	}

	m_native = std::make_unique<Native>();
	m_native->layer = layer_bits::pack(m_settings.layers, m_settings.mask, true);
	settings->mInnerBodyLayer = m_native->layer;
	m_native->update.mWalkStairsStepUp = JPH::Vec3(0, m_settings.stepHeight, 0);
	m_native->update.mStickToFloorStepDown = JPH::Vec3(0, -m_settings.stickToFloorDistance, 0);

	m_native->character = new JPH::CharacterVirtual(
		settings, bird::toJolt(object->getWorldPosition()), JPH::Quat::sIdentity(),
		reinterpret_cast<JPH::uint64>(this), &impl.system);
	m_native->character->SetCharacterVsCharacterCollision(&impl.characterVsCharacter);
	m_native->character->SetListener(&impl.characterContacts);
	impl.characterVsCharacter.Add(m_native->character);

	m_syncedRevision = object->getTransformRevision();
	m_index = impl.characters.size();
	impl.characters.push_back(this);
	impl.characterByInnerBody[_innerBodyId()] = this;
}

void CharacterControllerComponent::_deinit() {
	destroyCharacter(true);
}

void CharacterControllerComponent::destroyCharacter(bool notify) {
	if (m_world == nullptr) return;
	m_world->_removeCharacter(this, notify);  // ends its contact pairs, unregisters
	_forgetWorld();
}

void CharacterControllerComponent::_forgetWorld() {
	if (m_world == nullptr) return;
	// Destroying the CharacterVirtual removes its inner body: the system
	// must still exist here.
	m_world->_impl().characterVsCharacter.Remove(m_native->character);
	m_native.reset();
	m_world = nullptr;
}

uint32_t CharacterControllerComponent::_innerBodyId() const {
	return m_native ? m_native->character->GetInnerBodyID().GetIndexAndSequenceNumber()
					: JPH::BodyID::cInvalidBodyID;
}

void CharacterControllerComponent::requireWorld(const char* what) const {
	if (m_world == nullptr) {
		throw std::logic_error(std::string("CharacterControllerComponent::") + what +
							   ": component is not in a physics world");
	}
}

// ------------------------------------------------------------------- step

void CharacterControllerComponent::_step(float fixedDelta) {
	WorldObject* object = world_object();
	JPH::CharacterVirtual& character = *m_native->character;
	PhysicsWorldImpl& impl = m_world->_impl();

	if (object->getTransformRevision() != m_syncedRevision) {
		character.SetPosition(bird::toJolt(object->getWorldPosition()));  // game moved it
	}

	const JPH::Vec3 up = character.GetUp();
	const JPH::Vec3 gravity = gravityFor(m_world, m_settings.gravityFactor);
	JPH::Vec3 velocity;
	if (m_hasVelocityOverride) {
		velocity = bird::toJolt(m_velocityOverride);
		m_hasVelocityOverride = false;
	} else {
		// Standard Jolt character recipe: keep vertical momentum in the air,
		// ride the ground's velocity when standing, add gravity, then steer.
		character.UpdateGroundVelocity();
		const JPH::Vec3 vertical = up * character.GetLinearVelocity().Dot(up);
		const JPH::Vec3 ground = character.GetGroundVelocity();
		const bool standing = character.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround &&
							  (vertical - ground).Dot(up) < 0.1f;
		velocity = standing ? ground : vertical;
		if (standing && m_jumpRequested) velocity += up * m_jumpSpeed;
		velocity += gravity * fixedDelta;
		const JPH::Vec3 move = bird::toJolt(m_moveVelocity);
		velocity += move - up * move.Dot(up);
	}
	m_jumpRequested = false;
	character.SetLinearVelocity(velocity);

	m_native->touches.clear();
	character.ExtendedUpdate(fixedDelta, gravity, m_native->update,
							 impl.system.GetDefaultBroadPhaseLayerFilter(m_native->layer),
							 impl.system.GetDefaultLayerFilter(m_native->layer),
							 impl.innerBodyFilter, JPH::ShapeFilter(),
							 *impl.server->tempAllocator);
	diffTouches();

	object->setWorldPosition(toBird(JPH::Vec3(character.GetPosition())));
	m_syncedRevision = object->getTransformRevision();
}

// ---------------------------------------------------------------- runtime

void CharacterControllerComponent::setMoveVelocity(const Vector3& velocity) {
	m_moveVelocity = velocity;
}

bool CharacterControllerComponent::jump(float speed) {
	if (!isOnGround()) return false;
	m_jumpSpeed = speed;
	m_jumpRequested = true;
	return true;
}

Vector3 CharacterControllerComponent::getVelocity() const {
	requireWorld("getVelocity");
	return toBird(m_native->character->GetLinearVelocity());
}

void CharacterControllerComponent::setVelocity(const Vector3& velocity) {
	m_velocityOverride = velocity;
	m_hasVelocityOverride = true;
}

GroundState CharacterControllerComponent::getGroundState() const {
	requireWorld("getGroundState");
	switch (m_native->character->GetGroundState()) {
		case JPH::CharacterBase::EGroundState::OnGround:
			return GroundState::OnGround;
		case JPH::CharacterBase::EGroundState::OnSteepGround:
			return GroundState::OnSteepGround;
		case JPH::CharacterBase::EGroundState::NotSupported:
			return GroundState::NotSupported;
		default:
			return GroundState::InAir;
	}
}

bool CharacterControllerComponent::isOnGround() const {
	return getGroundState() == GroundState::OnGround;
}

Vector3 CharacterControllerComponent::getGroundNormal() const {
	requireWorld("getGroundNormal");
	if (m_native->character->GetGroundState() == JPH::CharacterBase::EGroundState::InAir) {
		return Vector3(0.0f);
	}
	return toBird(m_native->character->GetGroundNormal());
}

RigidBodyComponent* CharacterControllerComponent::getGroundBody() const {
	requireWorld("getGroundBody");
	const PhysicsWorldImpl& impl = m_world->_impl();
	auto it = impl.byId.find(m_native->character->GetGroundBodyID().GetIndexAndSequenceNumber());
	return it == impl.byId.end() ? nullptr : it->second;
}

std::vector<RigidBodyComponent*> CharacterControllerComponent::getTouchingBodies() const {
	requireWorld("getTouchingBodies");
	const PhysicsWorldImpl& impl = m_world->_impl();
	std::vector<RigidBodyComponent*> bodies;
	std::unordered_set<RigidBodyComponent*> seen;
	for (const auto& contact : m_native->character->GetActiveContacts()) {
		if (!contact.mHadCollision) continue;
		auto it = impl.byId.find(contact.mBodyB.GetIndexAndSequenceNumber());
		if (it != impl.byId.end() && seen.insert(it->second).second) bodies.push_back(it->second);
	}
	return bodies;
}

void CharacterControllerComponent::teleport(const Vector3& position) {
	requireWorld("teleport");
	world_object()->setWorldPosition(position);
	m_native->character->SetPosition(bird::toJolt(position));
	m_syncedRevision = world_object()->getTransformRevision();
}

// --------------------------------------------------------------- contacts

void PhysicsWorldImpl::CharacterContactRecorder::record(const JPH::CharacterVirtual* character,
														  const JPH::BodyID& body,
														  JPH::RVec3Arg point,
														  JPH::Vec3Arg normal) {
	auto it = m_impl.byId.find(body.GetIndexAndSequenceNumber());
	if (it == m_impl.byId.end()) return;  // not a component's body
	auto* self = reinterpret_cast<CharacterControllerComponent*>(character->GetUserData());
	self->_recordTouch(it->first, it->second->getSettings().isSensor, false,
					   toBird(JPH::Vec3(point)), toBird(normal));
}

void PhysicsWorldImpl::CharacterContactRecorder::recordCharacter(
	const JPH::CharacterVirtual* character, const JPH::CharacterVirtual* other,
	JPH::RVec3Arg point, JPH::Vec3Arg normal) {
	auto* self = reinterpret_cast<CharacterControllerComponent*>(character->GetUserData());
	auto* otherComponent =
		reinterpret_cast<CharacterControllerComponent*>(other->GetUserData());
	self->_recordTouch(otherComponent->_innerBodyId(), false, true, toBird(JPH::Vec3(point)),
					   toBird(normal));
}

void CharacterControllerComponent::_recordTouch(uint32_t otherId, bool sensor,
												bool otherIsCharacter, const Vector3& point,
												const Vector3& normal) {
	// Several sub-shape contacts with one body collapse into one touch.
	m_native->touches.emplace(otherId, Native::Touch{sensor, otherIsCharacter, point, normal});
}

void CharacterControllerComponent::diffTouches() {
	PhysicsWorldImpl& impl = m_world->_impl();
	const uint32_t self = _innerBodyId();
	for (const auto& [other, touch] : m_native->touches) {
		const PhysicsWorldImpl::CharacterPairState state{touch.sensor, touch.otherIsCharacter};
		if (impl.characterPairs.emplace(std::make_pair(self, other), state).second) {
			impl.characterEvents.push_back({true, self, other, state, touch.point, touch.normal});
		}
	}
	auto it = impl.characterPairs.lower_bound({self, 0});
	while (it != impl.characterPairs.end() && it->first.first == self) {
		if (m_native->touches.count(it->first.second) == 0) {
			impl.characterEvents.push_back(
				{false, self, it->first.second, it->second, Vector3(0.0f), Vector3(0.0f)});
			it = impl.characterPairs.erase(it);
		} else {
			++it;
		}
	}
}

std::vector<CharacterControllerComponent::ContactCallback> CharacterControllerComponent::_receivers(
	ContactEventKind kind) const {
	switch (kind) {
		case ContactEventKind::ContactBegin:
			return collectContactReceivers(m_object, kind, m_onContactBegin);
		case ContactEventKind::ContactEnd:
			return collectContactReceivers(m_object, kind, m_onContactEnd);
		case ContactEventKind::TriggerEnter:
			return collectContactReceivers(m_object, kind, m_onTriggerEnter);
		default:
			return collectContactReceivers(m_object, kind, m_onTriggerExit);
	}
}

CharacterControllerComponent& CharacterControllerComponent::setOnContactBegin(
	ContactCallback callback) {
	m_onContactBegin = std::move(callback);
	return *this;
}

CharacterControllerComponent& CharacterControllerComponent::setOnContactEnd(
	ContactCallback callback) {
	m_onContactEnd = std::move(callback);
	return *this;
}

CharacterControllerComponent& CharacterControllerComponent::setOnTriggerEnter(
	ContactCallback callback) {
	m_onTriggerEnter = std::move(callback);
	return *this;
}

CharacterControllerComponent& CharacterControllerComponent::setOnTriggerExit(
	ContactCallback callback) {
	m_onTriggerExit = std::move(callback);
	return *this;
}

}  // namespace bird
