#include "PhysicsPipeline.h"

#include <components/PhysicsBodyComponent.h>

#include <memory>
#include <vector>

#include "JoltIncludes.h"

namespace bird {

using namespace JPH;
// 1. BroadPhaseLayerInterface: Maps Object Layers -> BroadPhase Layers
class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
   public:
	BPLayerInterfaceImpl() {
		// Create a mapping table from ObjectLayer to BroadPhaseLayer
		mObjectToBroadPhase[ObjectLayers::NON_MOVING] =
				BroadPhaseLayer(BroadPhaseLayers::NON_MOVING);
		mObjectToBroadPhase[ObjectLayers::MOVING] = BroadPhaseLayer(BroadPhaseLayers::MOVING);
	}

	virtual uint32 GetNumBroadPhaseLayers() const override {
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer inLayer) const override {
		return BroadPhaseLayer(mObjectToBroadPhase[inLayer]);
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(BroadPhaseLayer inLayer) const override {
		switch ((BroadPhaseLayer::Type)inLayer) {
			case (BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
				return "NON_MOVING";
			case (BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
				return "MOVING";
			default:
				return "INVALID";
		}
	}
#endif

   private:
	BroadPhaseLayer mObjectToBroadPhase[ObjectLayers::NUM_LAYERS];
};

// 2. ObjectVsBroadPhaseLayerFilter: Can an Object collide with a BroadPhase
// Layer?
class ObjectVsBPLayerFilterImpl : public ObjectVsBroadPhaseLayerFilter {
   public:
	virtual bool ShouldCollide(ObjectLayer inLayer1, BroadPhaseLayer inLayer2) const override {
		switch (inLayer1) {
			case ObjectLayers::NON_MOVING:
				// Static objects collide with Moving broadphase
				return inLayer2 == BroadPhaseLayer(BroadPhaseLayers::MOVING);
			case ObjectLayers::MOVING:
				// Moving objects collide with everything
				return true;
			default:
				return false;
		}
	}
};

// 3. ObjectLayerPairFilter: Can Object A collide with Object B?
class ObjectLayerPairFilterImpl : public ObjectLayerPairFilter {
   public:
	virtual bool ShouldCollide(ObjectLayer inObject1, ObjectLayer inObject2) const override {
		switch (inObject1) {
			case ObjectLayers::NON_MOVING:
				return inObject2 == ObjectLayers::MOVING;  // Static only hits moving
			case ObjectLayers::MOVING:
				return true;  // Moving hits everything
			default:
				return false;
		}
	}
};

// ===================================================================================
// Physics System Implementation
// ===================================================================================
PhysicsPipeline::PhysicsPipeline() {
	RegisterDefaultAllocator();
	Factory::sInstance = new Factory();
	RegisterTypes();

	m_TempAllocator = std::make_unique<TempAllocatorImpl>(10 * 1024 * 1024);  // 10MB
	m_JobSystem = std::make_unique<JobSystemThreadPool>(
			cMaxPhysicsJobs, cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	m_BPLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
	m_ObjectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBPLayerFilterImpl>();
	m_ObjectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();
}
PhysicsPipeline::~PhysicsPipeline() = default;

void PhysicsPipeline::init() {
	m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
	m_PhysicsSystem->Init(1024,	 // Max Bodies
			0,					 // Num Mutexes
			1024,				 // Max Body Pairs
			1024,				 // Max Contact Constraints
			*m_BPLayerInterface, *m_ObjectVsBroadPhaseLayerFilter, *m_ObjectLayerPairFilter);

	BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();

	BoxShapeSettings floorShapeSettings(Vec3(100.0f, 1.0f, 100.0f));
	ShapeSettings::ShapeResult shapeResult = floorShapeSettings.Create();

	BodyCreationSettings floorSettings(shapeResult.Get(), RVec3(0.0f, -5.0f, 0.0f),
			Quat::sIdentity(), EMotionType::Static, ObjectLayers::NON_MOVING);

	bodyInterface.CreateAndAddBody(floorSettings, EActivation::Activate);
}

void PhysicsPipeline::cleanup() {
	delete Factory::sInstance;
	Factory::sInstance = nullptr;
}

void PhysicsPipeline::update(float delta) {
	const int cCollisionSteps = 1;
	m_PhysicsSystem->Update(delta, cCollisionSteps, m_TempAllocator.get(), m_JobSystem.get());

	BodyInterface& bodyInterface = m_PhysicsSystem->GetBodyInterface();
	for (auto& body : m_physicsBodies) {
		BodyID id = BodyID(body->getBodyID());
		WorldObject* obj = body->getParent();

		if (bodyInterface.IsActive(id)) {
			JPH::RVec3 pos = bodyInterface.GetPosition(id);
			JPH::Quat rot = bodyInterface.GetRotation(id);

			body->m_physicsUpdateTransfer = true;
			obj->setWorldPosition(Vector3(pos.GetX(), pos.GetY(), pos.GetZ()));
			obj->setWorldRotation(Quaternion(rot.GetX(), rot.GetY(), rot.GetZ(), rot.GetW()));
			body->m_physicsUpdateTransfer = false;
		}
	}
}

void PhysicsPipeline::addBody(PhysicsBodyComponent* body) {
	m_physicsBodies.emplace_back(body);
}

void PhysicsPipeline::removeBody(PhysicsBodyComponent* body) {
	m_physicsBodies.erase(std::remove(m_physicsBodies.begin(), m_physicsBodies.end(), body),
			m_physicsBodies.end());
}

JPH::BodyInterface& PhysicsPipeline::getBodyInterface() {
	return m_PhysicsSystem->GetBodyInterface();
}

}  // namespace bird
