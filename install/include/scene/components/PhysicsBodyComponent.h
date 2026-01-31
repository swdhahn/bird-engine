#ifndef BIRD_PHYSICSBODY_H
#define BIRD_PHYSICSBODY_H

#include "Component.h"

namespace bird {

// multiple collider shapes?
class PhysicsBodyComponent : public Component {
   public:
	PhysicsBodyComponent();
	~PhysicsBodyComponent();

	virtual void init() = 0;
	virtual void deinit() = 0;

	uint32_t getBodyID();
	void _setBodyID(uint32_t bodyID);

   private:
	uint32_t m_bodyID;
};

}  // namespace bird

#endif	// !BIRD_PHYSICSBODY_H
