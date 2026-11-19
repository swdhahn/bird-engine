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
typedef void (*BirdDartDeinitFn)(void* hostWorldObject);

/**
 * Creates a DartScriptComponent backed by Dart NativeCallable callbacks.
 * Ownership transfers to C++ when attached via WorldObject::addComponent.
 */
BIRD_DART_C_API void* bird_DartScriptComponent_new(
	BirdDartInitFn onInit, BirdDartProcessFn onProcess,
	BirdDartDeinitFn onDeinit);
}

namespace bird {

/**
 * ProcessScriptComponent bridge that dispatches init(), process(delta), and
 * deinit() directly into Dart NativeCallable callbacks on the main isolate.
 */
class DartScriptComponent : public ProcessScriptComponent {
   public:
	DartScriptComponent(BirdDartInitFn onInit, BirdDartProcessFn onProcess,
						BirdDartDeinitFn onDeinit);
	~DartScriptComponent() override = default;

	void init() override;
	void process(float delta) override;
	void deinit() override;

   private:
	BirdDartInitFn m_onInit;
	BirdDartProcessFn m_onProcess;
	BirdDartDeinitFn m_onDeinit;
	Dart_Isolate m_isolate;
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
