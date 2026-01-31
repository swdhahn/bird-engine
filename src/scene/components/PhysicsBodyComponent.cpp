#include "PhysicsBodyComponent.h"

#include "../../physics/JoltIncludes.h"
#include "../Application.h"
#include "Component.h"

using namespace JPH;

namespace bird {

PhysicsBodyComponent::PhysicsBodyComponent()
	: m_bodyID(0xffffffff), m_physicsUpdateTransfer(false) {}

PhysicsBodyComponent::~PhysicsBodyComponent() {}

void PhysicsBodyComponent::init() {
	std::cout << "Physics Body Missing Collider Shapes!" << std::endl;
	BodyInterface& bodyInterface = APPLICATION->getPhysicsPipeline()->getBodyInterface();

	auto pos = m_parent->getWorldPosition();
	auto rot = m_parent->getWorldRotation();
	auto s = m_parent->getScale();

	BoxShapeSettings boxShapeSettings(Vec3(s.x, s.y, s.z));
	ShapeSettings::ShapeResult shapeResult = boxShapeSettings.Create();

	BodyCreationSettings boxSettings(shapeResult.Get(), RVec3(pos.x, pos.y, pos.z),
			Quat(rot.x, rot.y, rot.z, rot.w), EMotionType::Dynamic, ObjectLayers::MOVING);
	BodyID id = bodyInterface.CreateAndAddBody(boxSettings, EActivation::Activate);
	m_bodyID = id.GetIndexAndSequenceNumber();
	APPLICATION->getPhysicsPipeline()->addBody(this);
}

void PhysicsBodyComponent::deinit() {
	BodyInterface& bodyInterface = APPLICATION->getPhysicsPipeline()->getBodyInterface();
	APPLICATION->getPhysicsPipeline()->removeBody(this);
	bodyInterface.RemoveBody(BodyID(m_bodyID));
	bodyInterface.DestroyBody(BodyID(m_bodyID));
}

void PhysicsBodyComponent::updateTransform() {
	BodyInterface& bodyInterface = APPLICATION->getPhysicsPipeline()->getBodyInterface();
	BodyID id = BodyID(m_bodyID);
	auto pos = m_parent->getWorldPosition();
	auto rot = m_parent->getWorldRotation();
	bodyInterface.SetPosition(id, Vec3(pos.x, pos.y, pos.z), EActivation::Activate);
	bodyInterface.SetRotation(id, Quat(rot.x, rot.y, rot.z, rot.w), EActivation::Activate);
}

uint32_t PhysicsBodyComponent::getBodyID() {
	return m_bodyID;
}

}  // namespace bird
