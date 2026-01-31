//
// Created by Seth Hahn on 11/30/22.
//

#ifndef BIRD_ENTITY_H
#define BIRD_ENTITY_H

#include "WorldObject.h"

namespace bird {
class Scene;

class Entity : public WorldObject {
   public:
	Entity();
	virtual ~Entity() {};

	virtual void process(float delta) = 0;

	void setScene(Scene* scene);
	Scene* getScene();

   protected:
	Scene* m_scene;
};

}  // namespace bird

#endif	// BIRD_ENTITY_H
