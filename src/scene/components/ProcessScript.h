#ifndef H_BIRD_PROCESS_SCRIPT_COMPONENT
#define H_BIRD_PROCESS_SCRIPT_COMPONENT

#include <memory>

#include "Script.h"

namespace bird {

/**
 * Script component receiving per-frame update ticks.
 *
 * Automatically registers with the central Application in _init() and unregisters
 * in _deinit(), receiving process(float delta) updates every frame before rendering.
 */
class ProcessScriptComponent : public ScriptComponent {
   public:
	void _init() override;
	void _deinit() override;

	/**
	 * Per-frame update callback.
	 *
	 * Called every frame from Application::run() before scene matrix updates
	 * and rendering passes.
	 *
	 * @param delta Elapsed time since the last frame in seconds.
	 */
	virtual void process(float delta);

	/**
	 * Fixed-rate update, called zero or more times per frame just before
	 * each physics step (see FixedStepClock). Put forces and other
	 * simulation-coupled logic here.
	 *
	 * @param fixedDelta The fixed step length in seconds.
	 */
	virtual void physicsProcess(float fixedDelta);
};

/**
 * Strongly-typed per-frame ticked script template.
 *
 * @tparam T Host entity type inheriting from Object (e.g. Camera, WorldObject).
 */
template <typename T>
class ProcessScript : public ProcessScriptComponent {
   public:
	/** Returns the host entity cast to T*. */
	T* object() {
		return (T*)world_object();
	}
};

}  // namespace bird

#endif	// H_BIRD_PROCESS_SCRIPT_COMPONENT
