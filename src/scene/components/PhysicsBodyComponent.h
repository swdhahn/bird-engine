#ifndef BIRD_PHYSICSBODY_H
#define BIRD_PHYSICSBODY_H

#include "Component.h"

namespace bird {

// multiple collider shapes?
class PhysicsBodyComponent : public Component {
   public:
	PhysicsBodyComponent();
	~PhysicsBodyComponent();

	void init() override;
	void deinit() override;

	void updateTransform();

	uint32_t getBodyID();
	void _setBodyID(uint32_t bodyID);
	bool m_physicsUpdateTransfer;

   private:
	uint32_t m_bodyID;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSBODY_H
