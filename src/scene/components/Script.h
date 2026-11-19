#ifndef H_BIRD_SCRIPT_COMPONENT
#define H_BIRD_SCRIPT_COMPONENT

#include <memory>

#include "Component.h"

namespace bird {

/**
 * Script component providing convenient access to engine singletons and host state.
 *
 * Exposes helper accessors for the parent Scene, ResourceManager, and Input
 * systems. Serves as the foundation for user gameplay logic and application entrypoints.
 */
class ScriptComponent : public Component {
   public:
	virtual void init() override;
	virtual void deinit() override;

   protected:
	/** Returns the enclosing parent Scene of the host entity. */
	Scene* scene();
	/** Returns the engine ResourceManager singleton for loading assets. */
	ResourceManager* resource_manager();
	/** Returns the engine Input singleton for querying actions and peripherals. */
	Input* input();

	/** Returns the component collection attached to the host entity. */
	const std::vector<std::unique_ptr<Component>>& components();
	/** Adds a component to the host entity. */
	void addComponent(std::unique_ptr<Component> component);
	/** Removes a component from the host entity. */
	void removeComponent(Component* component);
};

/**
 * Strongly-typed script template.
 *
 * Binds object() to return a typed pointer to host type T (e.g. Scene, WorldObject).
 *
 * @tparam T Host entity type inheriting from Object.
 */
template <typename T>
class Script : public ScriptComponent {
   public:
	/** Returns the host entity cast to T*. */
	T* object() {
		return (T*)world_object();
	}
};

}  // namespace bird

#endif	// H_BIRD_SCRIPT_COMPONENT
