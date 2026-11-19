#include "DartScriptComponent.h"

#include <dart_api.h>

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

DartScriptComponent::DartScriptComponent(BirdDartInitFn onInit,
										 BirdDartProcessFn onProcess,
										 BirdDartDeinitFn onDeinit)
	: m_onInit(onInit),
	  m_onProcess(onProcess),
	  m_onDeinit(onDeinit),
	  m_isolate(Dart_CurrentIsolate()) {}

void DartScriptComponent::init() {
	if (m_onInit) {
		ScopedIsolateEntry guard(m_isolate);
		m_onInit(static_cast<void*>(world_object()));
	}
}

void DartScriptComponent::process(float delta) {
	if (m_onProcess) {
		ScopedIsolateEntry guard(m_isolate);
		m_onProcess(static_cast<void*>(world_object()), delta);
	}
}

void DartScriptComponent::deinit() {
	if (m_onDeinit) {
		ScopedIsolateEntry guard(m_isolate);
		m_onDeinit(static_cast<void*>(world_object()));
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
	BirdDartDeinitFn onDeinit) {
	return new bird::DartScriptComponent(onInit, onProcess, onDeinit);
}

}  // extern "C"
