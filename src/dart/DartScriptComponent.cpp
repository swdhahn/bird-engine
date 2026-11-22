#include "DartScriptComponent.h"

#include <dart_api.h>

#include <exception>

#include "../diagnostics/Logger.h"

namespace bird {

namespace {
struct ScopedIsolateEntry {
	bool entered = false;
	explicit ScopedIsolateEntry(Dart_Isolate isolate) {
		if (isolate && Dart_CurrentIsolate() == nullptr) {
			Dart_EnterIsolate(isolate);
			entered = true;
		}
	}
	~ScopedIsolateEntry() {
		if (entered) {
			Dart_ExitIsolate();
		}
	}
};
}  // namespace

DartScriptComponent::DartScriptComponent(
	BirdDartInitFn onInit, BirdDartProcessFn onProcess,
	BirdDartPhysicsProcessFn onPhysicsProcess, BirdDartDeinitFn onDeinit)
	: m_onInit(onInit),
	  m_onProcess(onProcess),
	  m_onPhysicsProcess(onPhysicsProcess),
	  m_onDeinit(onDeinit),
	  m_isolate(Dart_CurrentIsolate()) {}

void DartScriptComponent::disable(const std::string& reason) {
	if (m_disabled) return;
	m_disabled = true;
	m_disableReason = reason;
	BIRD_LOG_ERROR("DartScript",
				   "Disabling faulted DartScriptComponent: " +
					   (reason.empty() ? "unhandled script error" : reason));
}

void DartScriptComponent::init() {
	if (m_disabled || !m_onInit) return;
	try {
		ScopedIsolateEntry guard(m_isolate);
		m_onInit(static_cast<void*>(world_object()));
	} catch (const std::exception& e) {
		disable(std::string("Exception in init(): ") + e.what());
	} catch (...) {
		disable("Unknown exception in init()");
	}
}

void DartScriptComponent::process(float delta) {
	if (m_disabled || !m_onProcess) return;
	try {
		ScopedIsolateEntry guard(m_isolate);
		m_onProcess(static_cast<void*>(world_object()), delta);
	} catch (const std::exception& e) {
		disable(std::string("Exception in process(): ") + e.what());
	} catch (...) {
		disable("Unknown exception in process()");
	}
}

void DartScriptComponent::physicsProcess(float fixedDelta) {
	if (m_disabled || !m_onPhysicsProcess) return;
	try {
		ScopedIsolateEntry guard(m_isolate);
		m_onPhysicsProcess(static_cast<void*>(world_object()), fixedDelta);
	} catch (const std::exception& e) {
		disable(std::string("Exception in physicsProcess(): ") + e.what());
	} catch (...) {
		disable("Unknown exception in physicsProcess()");
	}
}

void DartScriptComponent::deinit() {
	if (!m_onDeinit) return;
	// Always invoke m_onDeinit even if disabled so Dart closes its
	// NativeCallables cleanly; Dart's error boundary skips user deinit() if
	// already disabled and still releases native callback handles.
	try {
		ScopedIsolateEntry guard(m_isolate);
		m_onDeinit(static_cast<void*>(world_object()));
	} catch (const std::exception& e) {
		disable(std::string("Exception in deinit(): ") + e.what());
	} catch (...) {
		disable("Unknown exception in deinit()");
	}
}

DartAppScript::DartAppScript(DartRuntime* runtime) : m_runtime(runtime) {}

void DartAppScript::init() {
	if (!m_runtime) return;
	m_runtime->invokeTopLevelPtr("appEntry", static_cast<void*>(object()),
								 /*optional=*/false);
}

void DartAppScript::deinit() {
	if (!m_runtime) return;
	m_runtime->invokeTopLevelVoid("appDeinit", /*optional=*/true);
}

}  // namespace bird

extern "C" {

BIRD_DART_C_API void* bird_DartScriptComponent_new(
	BirdDartInitFn onInit, BirdDartProcessFn onProcess,
	BirdDartPhysicsProcessFn onPhysicsProcess, BirdDartDeinitFn onDeinit) {
	return new bird::DartScriptComponent(onInit, onProcess, onPhysicsProcess,
										 onDeinit);
}

BIRD_DART_C_API void bird_DartScriptComponent_setMetadata(
	void* component, const char* scriptClass, const char* scriptPath) {
	if (!component) return;
	auto* comp = static_cast<bird::DartScriptComponent*>(component);
	if (scriptClass) comp->setScriptClass(scriptClass);
	if (scriptPath) comp->setScriptPath(scriptPath);
}

BIRD_DART_C_API void bird_DartScriptComponent_setPropertyBool(
	void* component, const char* name, bool value) {
	if (!component || !name) return;
	static_cast<bird::DartScriptComponent*>(component)->setProperty(
		name, bird::SerializedValue(value));
}

BIRD_DART_C_API void bird_DartScriptComponent_setPropertyInt(
	void* component, const char* name, int64_t value) {
	if (!component || !name) return;
	static_cast<bird::DartScriptComponent*>(component)->setProperty(
		name, bird::SerializedValue(value));
}

BIRD_DART_C_API void bird_DartScriptComponent_setPropertyDouble(
	void* component, const char* name, double value) {
	if (!component || !name) return;
	static_cast<bird::DartScriptComponent*>(component)->setProperty(
		name, bird::SerializedValue(value));
}

BIRD_DART_C_API void bird_DartScriptComponent_setPropertyString(
	void* component, const char* name, const char* value) {
	if (!component || !name) return;
	static_cast<bird::DartScriptComponent*>(component)->setProperty(
		name, bird::SerializedValue(value ? std::string(value) : std::string()));
}

BIRD_DART_C_API void bird_DartScriptComponent_disable(void* componentPtr,
													  const char* reason) {
	if (!componentPtr) return;
	auto* comp = static_cast<bird::DartScriptComponent*>(componentPtr);
	comp->disable(reason ? std::string(reason) : std::string());
}

}  // extern "C"
