#ifndef BIRD_COMPONENT_H
#define BIRD_COMPONENT_H

#include "../WorldObject.h"

namespace bird {

/**
 * Type of Components to create
 *  - Physics
 *  - Mesh/Model
 *  - Audio
 *  - Animation
 *  - Light
 *
 *  General idea of Component
 *   - Attach to WorldObject
 *   - Component adds/removes itself from different systems accessed through
 * bird::APPLICATION
 *   - Example: APPLICATION->getGraphicsPipeline()->add_model/remove_model() --
 * This would add or remove it from rendering -> Could also implement it as a
 * enable/disable toggle
 *   - Example: APPLICATION->getPhysicsPipeline()->add_body/remove_body() --
 * same as above
 */

class Component {
   public:
	Component();
	virtual ~Component() {}

	virtual void init() = 0;
	virtual void deinit() = 0;

	virtual void process(float delta) = 0;

	WorldObject* getParent();
	void setParent(WorldObject* parent);

   protected:
	WorldObject* m_parent;
};

}  // namespace bird

#endif	// BIRD_COMPONENT_H
