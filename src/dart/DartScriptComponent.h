#ifndef BIRD_DART_SCRIPT_COMPONENT_H
#define BIRD_DART_SCRIPT_COMPONENT_H

#include "../scene/Scene.h"
#include "../scene/components/ProcessScript.h"
#include "../scene/components/Script.h"
#include "DartRuntime.h"

#if defined(_WIN32) || defined(_MSC_VER)
#define BIRD_DART_C_API __declspec(dllexport)
#else
#define BIRD_DART_C_API __attribute__((visibility("default")))
#endif

extern "C" {
typedef void (*BirdDartInitFn)(void* hostWorldObject);
typedef void (*BirdDartProcessFn)(void* hostWorldObject, float delta);
typedef void (*BirdDartPhysicsProcessFn)(void* hostWorldObject,
										 float fixedDelta);
typedef void (*BirdDartDeinitFn)(void* hostWorldObject);

/**
 * Creates a DartScriptComponent backed by Dart NativeCallable callbacks.
 * Ownership transfers to C++ when attached via WorldObject::addComponent.
 */
BIRD_DART_C_API void* bird_DartScriptComponent_new(
	BirdDartInitFn onInit, BirdDartProcessFn onProcess,
	BirdDartPhysicsProcessFn onPhysicsProcess, BirdDartDeinitFn onDeinit);

/**
 * Disables a single DartScriptComponent after an unhandled Dart exception so
 * subsequent lifecycle callbacks are skipped without crashing the engine.
 *
 * @param componentPtr Pointer previously returned by `bird_DartScriptComponent_new`.
 * @param reason       Optional UTF-8 error summary (may be null).
 */
BIRD_DART_C_API void bird_DartScriptComponent_disable(void* componentPtr,
													  const char* reason);
}

namespace bird {

/**
 * ProcessScriptComponent bridge that dispatches init(), process(delta),
 * physicsProcess(fixedDelta), and deinit() directly into Dart NativeCallable
 * callbacks on the main isolate.
 */
class DartScriptComponent : public ProcessScriptComponent {
   public:
	DartScriptComponent(BirdDartInitFn onInit, BirdDartProcessFn onProcess,
						BirdDartPhysicsProcessFn onPhysicsProcess,
						BirdDartDeinitFn onDeinit);
	~DartScriptComponent() override = default;

	void init() override;
	void process(float delta) override;
	void physicsProcess(float fixedDelta) override;
	void deinit() override;

	/**
	 * Marks this script component as faulted/disabled so no further `init`,
	 * `process`, or `physicsProcess` callbacks are dispatched to Dart.
	 *
	 * @param reason Human-readable explanation of the fault.
	 */
	void disable(const std::string& reason);

	/** Returns true if this script component was disabled after an error. */
	bool isDisabled() const { return m_disabled; }

	/** Returns the fault reason recorded when `disable()` was called. */
	const std::string& disableReason() const { return m_disableReason; }

   private:
	BirdDartInitFn m_onInit;
	BirdDartProcessFn m_onProcess;
	BirdDartPhysicsProcessFn m_onPhysicsProcess;
	BirdDartDeinitFn m_onDeinit;
	Dart_Isolate m_isolate;
	bool m_disabled = false;
	std::string m_disableReason;
};

/**
 * Entrypoint Script<Scene> used by `Bird <script.dart>` to boot a Dart
 * application inside Application::init().
 */
class DartAppScript : public Script<Scene> {
   public:
	explicit DartAppScript(DartRuntime* runtime);
	~DartAppScript() override = default;

	void init() override;
	void deinit() override;

   private:
	DartRuntime* m_runtime;
};

}  // namespace bird

#endif	// BIRD_DART_SCRIPT_COMPONENT_H
