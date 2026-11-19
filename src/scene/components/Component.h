#ifndef BIRD_COMPONENT_H
#define BIRD_COMPONENT_H

#include "../WorldObject.h"

namespace bird {

/**
 * Base class for modular behaviors, data, and attachments on a WorldObject.
 *
 * Components follow an init/deinit lifecycle tied to their host entity.
 * Subclasses include MeshComponent (rendering) and Script/ProcessScript
 * (gameplay logic). Components can access their parent entity via world_object()
 * or strongly-typed downcast via objectAs<T>().
 */
class Component {
   public:
	Component();
	virtual ~Component() {}

	/**
	 * Internal framework hook called immediately before init().
	 * Used by engine-level subclasses (such as ProcessScriptComponent) to
	 * register with subsystem dispatchers before user initialization runs.
	 */
	virtual void _init();

	/**
	 * Internal framework hook called immediately after deinit().
	 * Used by engine-level subclasses to unregister from subsystem dispatchers.
	 */
	virtual void _deinit();

	/**
	 * Lifecycle hook called when the component is attached to an initialized
	 * entity or when the host entity is initialized into a scene.
	 */
	virtual void init() = 0;

	/**
	 * Lifecycle hook called when the component is removed or its host entity
	 * is deinitialized.
	 */
	virtual void deinit() = 0;

	/** Returns a raw observer pointer to the host WorldObject. */
	WorldObject* world_object();
	void setWorldObject(WorldObject* parent);

	/**
	 * Downcasts the host WorldObject to a specific derived type T.
	 *
	 * Performs compile-time assertion that T inherits from Object, and
	 * runtime verification against T::s_type. Asserts in debug builds on mismatch.
	 *
	 * @tparam T Derived Object type (e.g. Camera, Scene).
	 * @return Pointer to host as T, or nullptr if unbound.
	 */
	template <typename T>
	T* objectAs();

   protected:
	WorldObject* m_object;
};

}  // namespace bird

#endif	// BIRD_COMPONENT_H
