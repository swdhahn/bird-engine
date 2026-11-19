#ifndef H_BIRD_SCRIPT_COMPONENT
#define H_BIRD_SCRIPT_COMPONENT

#include <memory>

#include "Component.h"

namespace bird {

class ScriptComponent : public Component {
   public:
	virtual void init();
	virtual void deinit();

   protected:
	// Parent scene of this WorldObject
	Scene* scene();
	// Bird's ResourceManager singleton
	ResourceManager* resource_manager();
	// Bird's Input singleton
	Input* input();

	// List of components in the WorldObject this is attached to
	const std::vector<std::unique_ptr<Component>>& components();
	// Add a component to the WorldObject this is attached to
	void addComponent(std::unique_ptr<Component> component);
	// Remove a component to the WorldObject this is attached to
	void removeComponent(Component* component);
};

template <typename T>
class Script : public ScriptComponent {
   public:
	T* object() {
		return (T*)world_object();
	}
};

}  // namespace bird

#endif	// H_BIRD_SCRIPT_COMPONENT
