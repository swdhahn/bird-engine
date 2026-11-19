#include "Joint.h"

#include <algorithm>
#include <stdexcept>

#include "PhysicsInternal.h"
#include "PhysicsWorld.h"
#include "RigidBodyComponent.h"

namespace bird {

Joint::Joint(JointType type, RigidBodyComponent* a, RigidBodyComponent* b,
			 std::unique_ptr<Native> native, PhysicsWorld* world)
	: m_type(type), m_a(a), m_b(b), m_native(std::move(native)), m_world(world) {}

Joint::~Joint() {
	m_world->_impl().system.RemoveConstraint(m_native->constraint);
}

void Joint::setEnabled(bool enabled) {
	m_native->constraint->SetEnabled(enabled);
	m_world->_impl().system.GetBodyInterface().ActivateConstraint(m_native->constraint);
}

bool Joint::isEnabled() const {
	return m_native->constraint->GetEnabled();
}

void Joint::setMotorVelocity(float velocity, float maxForce) {
	JPH::TwoBodyConstraint* c = m_native->constraint;
	if (m_type == JointType::Hinge) {
		auto* hinge = static_cast<JPH::HingeConstraint*>(c);
		hinge->GetMotorSettings().SetTorqueLimit(maxForce);
		hinge->SetMotorState(JPH::EMotorState::Velocity);
		hinge->SetTargetAngularVelocity(velocity);
	} else if (m_type == JointType::Slider) {
		auto* slider = static_cast<JPH::SliderConstraint*>(c);
		slider->GetMotorSettings().SetForceLimit(maxForce);
		slider->SetMotorState(JPH::EMotorState::Velocity);
		slider->SetTargetVelocity(velocity);
	} else {
		throw std::logic_error("Joint::setMotorVelocity: only hinges and sliders have motors");
	}
	m_world->_impl().system.GetBodyInterface().ActivateConstraint(c);
}

void Joint::disableMotor() {
	if (m_type == JointType::Hinge) {
		static_cast<JPH::HingeConstraint*>(m_native->constraint.GetPtr())
			->SetMotorState(JPH::EMotorState::Off);
	} else if (m_type == JointType::Slider) {
		static_cast<JPH::SliderConstraint*>(m_native->constraint.GetPtr())
			->SetMotorState(JPH::EMotorState::Off);
	}
}

float Joint::getPosition() const {
	if (m_type == JointType::Hinge) {
		return static_cast<const JPH::HingeConstraint*>(m_native->constraint.GetPtr())
			->GetCurrentAngle();
	}
	if (m_type == JointType::Slider) {
		return static_cast<const JPH::SliderConstraint*>(m_native->constraint.GetPtr())
			->GetCurrentPosition();
	}
	throw std::logic_error("Joint::getPosition: only hinges and sliders have a position");
}

// ------------------------------------------------------- world factories

namespace {

JPH::Vec3 unitAxis(const Vector3& axis) {
	const float length = glm::length(axis);
	if (!(length > 0.0f)) throw std::invalid_argument("PhysicsWorld joint: axis must be non-zero");
	return toJolt(axis / length);
}

}  // namespace

static Joint* addJoint(PhysicsWorld* world, JointType type, RigidBodyComponent* a,
					   RigidBodyComponent* b, const JPH::TwoBodyConstraintSettings& settings,
					   bool aSecond = false) {
	if (a == nullptr || a->getWorld() != world || (b != nullptr && b->getWorld() != world) ||
		a == b) {
		throw std::invalid_argument(
			"PhysicsWorld joint: bodies must be distinct and in this world (b may be null)");
	}
	PhysicsWorldImpl& impl = world->_impl();
	JPH::BodyInterface& bodies = impl.system.GetBodyInterface();
	auto native = std::make_unique<Joint::Native>();
	const JPH::BodyID idA(a->_bodyId());
	const JPH::BodyID idB = b ? JPH::BodyID(b->_bodyId()) : JPH::BodyID();
	native->constraint = aSecond ? bodies.CreateConstraint(&settings, idB, idA)
								 : bodies.CreateConstraint(&settings, idA, idB);
	if (native->constraint == nullptr) {
		throw std::runtime_error("PhysicsWorld joint: Jolt could not create the constraint");
	}
	impl.system.AddConstraint(native->constraint);
	bodies.ActivateConstraint(native->constraint);
	impl.joints.push_back(std::make_unique<Joint>(type, a, b, std::move(native), world));
	return impl.joints.back().get();
}

Joint* PhysicsWorld::createFixedJoint(RigidBodyComponent* a, RigidBodyComponent* b) {
	JPH::FixedConstraintSettings s;
	s.mAutoDetectPoint = true;
	return addJoint(this, JointType::Fixed, a, b, s);
}

Joint* PhysicsWorld::createPointJoint(RigidBodyComponent* a, RigidBodyComponent* b,
									  const Vector3& anchor) {
	JPH::PointConstraintSettings s;
	s.mPoint1 = s.mPoint2 = toJolt(anchor);
	return addJoint(this, JointType::Point, a, b, s);
}

Joint* PhysicsWorld::createHingeJoint(RigidBodyComponent* a, RigidBodyComponent* b,
									  const Vector3& anchor, const Vector3& axis,
									  float minAngle, float maxAngle) {
	JPH::HingeConstraintSettings s;
	s.mPoint1 = s.mPoint2 = toJolt(anchor);
	s.mHingeAxis1 = s.mHingeAxis2 = unitAxis(axis);
	s.mNormalAxis1 = s.mNormalAxis2 = s.mHingeAxis1.GetNormalizedPerpendicular();
	s.mLimitsMin = std::max(minAngle, -JPH::JPH_PI);
	s.mLimitsMax = std::min(maxAngle, JPH::JPH_PI);
	return addJoint(this, JointType::Hinge, a, b, s);
}

Joint* PhysicsWorld::createSliderJoint(RigidBodyComponent* a, RigidBodyComponent* b,
									   const Vector3& axis, float minDistance,
									   float maxDistance) {
	JPH::SliderConstraintSettings s;
	s.mAutoDetectPoint = true;
	s.SetSliderAxis(unitAxis(axis));
	s.mLimitsMin = std::min(minDistance, 0.0f);
	s.mLimitsMax = std::max(maxDistance, 0.0f);
	// Jolt measures body 2 relative to body 1: put a second so positions and
	// limits read as a's own travel along the axis.
	return addJoint(this, JointType::Slider, a, b, s, true);
}

Joint* PhysicsWorld::createDistanceJoint(RigidBodyComponent* a, RigidBodyComponent* b,
										 const Vector3& anchorA, const Vector3& anchorB,
										 float minDistance, float maxDistance, float springHz) {
	JPH::DistanceConstraintSettings s;
	s.mPoint1 = toJolt(anchorA);
	s.mPoint2 = toJolt(anchorB);
	s.mMinDistance = minDistance;
	s.mMaxDistance = maxDistance;
	s.mLimitsSpringSettings.mFrequency = std::max(springHz, 0.0f);
	return addJoint(this, JointType::Distance, a, b, s);
}

void PhysicsWorld::destroyJoint(Joint* joint) {
	auto& joints = m_impl->joints;
	auto it = std::find_if(joints.begin(), joints.end(),
						   [joint](const std::unique_ptr<Joint>& j) { return j.get() == joint; });
	if (it == joints.end()) throw std::invalid_argument("PhysicsWorld::destroyJoint: unknown joint");
	joints.erase(it);
}

size_t PhysicsWorld::getJointCount() const {
	return m_impl->joints.size();
}

}  // namespace bird
