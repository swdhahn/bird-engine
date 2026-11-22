#ifndef BIRD_DART_SCRIPT_COMPONENT_H
#define BIRD_DART_SCRIPT_COMPONENT_H

#include <string>
#include <unordered_map>

#include "../scene/Scene.h"
#include "../scene/components/ProcessScript.h"
#include "../scene/components/Script.h"
#include "../serialization/SerializedValue.h"
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
 * Sets the Dart script class name and source path on a DartScriptComponent
 * for scene/prefab serialization.
 */
BIRD_DART_C_API void bird_DartScriptComponent_setMetadata(
	void* component, const char* scriptClass, const char* scriptPath);

/** Sets a string/double/int/bool exposed property on a DartScriptComponent. */
BIRD_DART_C_API void bird_DartScriptComponent_setPropertyBool(
	void* component, const char* name, bool value);
BIRD_DART_C_API void bird_DartScriptComponent_setPropertyInt(
	void* component, const char* name, int64_t value);
BIRD_DART_C_API void bird_DartScriptComponent_setPropertyDouble(
	void* component, const char* name, double value);
BIRD_DART_C_API void bird_DartScriptComponent_setPropertyString(
	void* component, const char* name, const char* value);
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

	/** Returns the Dart class name associated with this script component. */
	const std::string& getScriptClass() const { return m_scriptClass; }

	/** Sets the Dart class name for scene/prefab serialization. */
	void setScriptClass(std::string scriptClass) {
		m_scriptClass = std::move(scriptClass);
	}

	/** Returns the optional Dart source/package path for this script. */
	const std::string& getScriptPath() const { return m_scriptPath; }

	/** Sets the optional Dart source/package path for serialization. */
	void setScriptPath(std::string scriptPath) {
		m_scriptPath = std::move(scriptPath);
	}

	/** Sets or updates an exposed serializable property on this script. */
	void setProperty(const std::string& name, SerializedValue value) {
		m_properties[name] = std::move(value);
	}

	/** Returns the map of exposed serializable properties. */
	const std::unordered_map<std::string, SerializedValue>& getProperties() const {
		return m_properties;
	}

	std::unordered_map<std::string, SerializedValue>& getProperties() {
		return m_properties;
	}

   private:
	BirdDartInitFn m_onInit;
	BirdDartProcessFn m_onProcess;
	BirdDartPhysicsProcessFn m_onPhysicsProcess;
	BirdDartDeinitFn m_onDeinit;
	Dart_Isolate m_isolate;
	std::string m_scriptClass;
	std::string m_scriptPath;
	std::unordered_map<std::string, SerializedValue> m_properties;
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
