#ifndef BIRD_PHYSICSPIPELINE_H
#define BIRD_PHYSICSPIPELINE_H

#include <memory>
#include <vector>

#include "PhysicsLayers.h"
#include "glm/glm.hpp"

namespace JPH {
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystemThreadPool;
class BodyInterface;
}  // namespace JPH

namespace bird {

class BPLayerInterfaceImpl;
class ObjectVsBPLayerFilterImpl;
class ObjectLayerPairFilterImpl;

class PhysicsBodyComponent;

class PhysicsPipeline {
   public:
	PhysicsPipeline();
	~PhysicsPipeline();

	void init();
	void cleanup();

	void update(float delta);

	void addBody(PhysicsBodyComponent* body);
	void removeBody(PhysicsBodyComponent* body);

	JPH::BodyInterface& getBodyInterface();

   private:
	std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;
	std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
	std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;

	std::unique_ptr<BPLayerInterfaceImpl> m_BPLayerInterface;
	std::unique_ptr<ObjectVsBPLayerFilterImpl> m_ObjectVsBroadPhaseLayerFilter;
	std::unique_ptr<ObjectLayerPairFilterImpl> m_ObjectLayerPairFilter;

	std::vector<PhysicsBodyComponent*> m_physicsBodies;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSPIPELINE_H
