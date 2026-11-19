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

	// Always called before init(). This is mostly used for internal
	// functionality. Only subclasses of Script class actually use this. You may
	// use this if you're using the Script base class.
	virtual void _init();
	// Always called after deinit(). This is mostly used for internal
	// functionality. Only subclasses of Script class actually use this. You may
	// use this if you're using the Script base class.
	virtual void _deinit();
	virtual void init() = 0;
	virtual void deinit() = 0;

	// WorldObject this component is attached to
	WorldObject* world_object();
	void setWorldObject(WorldObject* parent);

	template <typename T>
	T* objectAs();

   protected:
	WorldObject* m_object;
};

}  // namespace bird

#endif	// BIRD_COMPONENT_H
