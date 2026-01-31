//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_ENTITY_H
#define BIRD_ENTITY_H

#include "WorldObject.h"

namespace bird {

class Entity : public WorldObject {
   public:
	Entity();
	virtual ~Entity() {};

	virtual void process(float delta) = 0;
};

}  // namespace bird

#endif	// BIRD_ENTITY_H
