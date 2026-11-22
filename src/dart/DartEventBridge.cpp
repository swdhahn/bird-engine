#include "DartEventBridge.h"

#include <dart_api.h>

#include <memory>
#include <unordered_set>

#include "../events/EventBus.h"
#include "../events/EventPayload.h"

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

struct DartEventBridgeState {
	Dart_Isolate isolate = nullptr;
	BirdDartEventDispatchFn onDispatch = nullptr;
	BirdDartEventUnsubscribeFn onUnsubscribe = nullptr;
	bird::EventBus* hookedBus = nullptr;
	std::unordered_set<bird::SubscriptionID> dartSubscriptions;
};

DartEventBridgeState& bridgeState() {
	static DartEventBridgeState state;
	return state;
}

void ensureUnsubscribeHookInstalled(bird::EventBus* bus) {
	auto& state = bridgeState();
	if (!bus || state.hookedBus == bus) return;
	state.hookedBus = bus;
	bus->setUnsubscribeHook([](bird::SubscriptionID id) {
		auto& st = bridgeState();
		auto it = st.dartSubscriptions.find(id);
		if (it == st.dartSubscriptions.end()) return;
		st.dartSubscriptions.erase(it);
		if (st.onUnsubscribe && st.isolate) {
			ScopedIsolateEntry guard(st.isolate);
			st.onUnsubscribe(id);
		}
	});
}

const bird::EventPayload* asConstPayload(const void* ptr) {
	return static_cast<const bird::EventPayload*>(ptr);
}

bird::EventPayload* asPayload(void* ptr) {
	return static_cast<bird::EventPayload*>(ptr);
}

}  // namespace

extern "C" {

BIRD_DART_EVENT_API void bird_events_register_callbacks(
	BirdDartEventDispatchFn onDispatch,
	BirdDartEventUnsubscribeFn onUnsubscribe) {
	auto& state = bridgeState();
	state.isolate = Dart_CurrentIsolate();
	state.onDispatch = onDispatch;
	state.onUnsubscribe = onUnsubscribe;
	if (bird::EventBus* bus = bird::EventBus::getGlobal()) {
		ensureUnsubscribeHookInstalled(bus);
	}
}

BIRD_DART_EVENT_API void bird_events_unregister_callbacks(void) {
	auto& state = bridgeState();
	bird::EventBus* bus = bird::EventBus::getGlobal();
	// Prevent re-entering Dart during teardown while still removing any
	// remaining Dart listeners from the native bus.
	state.onDispatch = nullptr;
	state.onUnsubscribe = nullptr;
	state.isolate = nullptr;

	if (bus) {
		if (state.hookedBus == bus) {
			bus->setUnsubscribeHook(nullptr);
		}
		auto subs = std::move(state.dartSubscriptions);
		state.dartSubscriptions.clear();
		for (bird::SubscriptionID id : subs) {
			bus->unsubscribe(id);
		}
	} else {
		state.dartSubscriptions.clear();
	}
	state.hookedBus = nullptr;
}

// ---------------------------------------------------------------------------
// EventPayload C ABI
// ---------------------------------------------------------------------------

BIRD_DART_EVENT_API void* bird_events_payload_new(void) {
	return new bird::EventPayload();
}

BIRD_DART_EVENT_API void bird_events_payload_delete(void* payloadPtr) {
	delete asPayload(payloadPtr);
}

BIRD_DART_EVENT_API void bird_events_payload_set_null(void* payloadPtr,
													  const char* key) {
	if (auto* p = asPayload(payloadPtr); p && key) p->setNull(key);
}

BIRD_DART_EVENT_API void bird_events_payload_set_bool(void* payloadPtr,
													  const char* key,
													  bool value) {
	if (auto* p = asPayload(payloadPtr); p && key) p->set(key, value);
}

BIRD_DART_EVENT_API void bird_events_payload_set_int(void* payloadPtr,
													 const char* key,
													 int64_t value) {
	if (auto* p = asPayload(payloadPtr); p && key) p->set(key, value);
}

BIRD_DART_EVENT_API void bird_events_payload_set_double(void* payloadPtr,
														const char* key,
														double value) {
	if (auto* p = asPayload(payloadPtr); p && key) p->set(key, value);
}

BIRD_DART_EVENT_API void bird_events_payload_set_string(void* payloadPtr,
														const char* key,
														const char* value) {
	if (auto* p = asPayload(payloadPtr); p && key) p->set(key, value);
}

BIRD_DART_EVENT_API void bird_events_payload_set_vec2(void* payloadPtr,
													  const char* key, float x,
													  float y) {
	if (auto* p = asPayload(payloadPtr); p && key) {
		p->set(key, bird::Vector2(x, y));
	}
}

BIRD_DART_EVENT_API void bird_events_payload_set_vec3(void* payloadPtr,
													  const char* key, float x,
													  float y, float z) {
	if (auto* p = asPayload(payloadPtr); p && key) {
		p->set(key, bird::Vector3(x, y, z));
	}
}

BIRD_DART_EVENT_API void bird_events_payload_set_vec4(void* payloadPtr,
													  const char* key, float x,
													  float y, float z,
													  float w) {
	if (auto* p = asPayload(payloadPtr); p && key) {
		p->set(key, bird::Vector4(x, y, z, w));
	}
}

BIRD_DART_EVENT_API uint32_t
bird_events_payload_field_count(const void* payloadPtr) {
	const auto* p = asConstPayload(payloadPtr);
	return p ? static_cast<uint32_t>(p->fields().size()) : 0u;
}

BIRD_DART_EVENT_API const char* bird_events_payload_field_key(
	const void* payloadPtr, uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return "";
	return p->fields()[index].key.c_str();
}

BIRD_DART_EVENT_API uint8_t
bird_events_payload_field_type(const void* payloadPtr, uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) {
		return static_cast<uint8_t>(bird::EventValueType::Null);
	}
	return static_cast<uint8_t>(
		bird::EventPayload::valueTypeOf(p->fields()[index].value));
}

BIRD_DART_EVENT_API bool bird_events_payload_field_bool(const void* payloadPtr,
														uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return false;
	if (const auto* v = std::get_if<bool>(&p->fields()[index].value)) {
		return *v;
	}
	return false;
}

BIRD_DART_EVENT_API int64_t
bird_events_payload_field_int(const void* payloadPtr, uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return 0;
	if (const auto* v = std::get_if<int64_t>(&p->fields()[index].value)) {
		return *v;
	}
	return 0;
}

BIRD_DART_EVENT_API double
bird_events_payload_field_double(const void* payloadPtr, uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return 0.0;
	if (const auto* v = std::get_if<double>(&p->fields()[index].value)) {
		return *v;
	}
	return 0.0;
}

BIRD_DART_EVENT_API const char* bird_events_payload_field_string(
	const void* payloadPtr, uint32_t index) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return "";
	if (const auto* v = std::get_if<std::string>(&p->fields()[index].value)) {
		return v->c_str();
	}
	return "";
}

BIRD_DART_EVENT_API void bird_events_payload_field_vec2(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return;
	if (const auto* v = std::get_if<bird::Vector2>(&p->fields()[index].value)) {
		if (outX) *outX = v->x;
		if (outY) *outY = v->y;
	}
}

BIRD_DART_EVENT_API void bird_events_payload_field_vec3(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY,
														float* outZ) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return;
	if (const auto* v = std::get_if<bird::Vector3>(&p->fields()[index].value)) {
		if (outX) *outX = v->x;
		if (outY) *outY = v->y;
		if (outZ) *outZ = v->z;
	}
}

BIRD_DART_EVENT_API void bird_events_payload_field_vec4(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY,
														float* outZ,
														float* outW) {
	const auto* p = asConstPayload(payloadPtr);
	if (!p || index >= p->fields().size()) return;
	if (const auto* v = std::get_if<bird::Vector4>(&p->fields()[index].value)) {
		if (outX) *outX = v->x;
		if (outY) *outY = v->y;
		if (outZ) *outZ = v->z;
		if (outW) *outW = v->w;
	}
}

// ---------------------------------------------------------------------------
// EventBus C ABI
// ---------------------------------------------------------------------------

BIRD_DART_EVENT_API bool bird_events_emit(const char* eventName,
										  const void* payloadPtr) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	if (!bus || !eventName || eventName[0] == '\0') return false;
	if (const auto* p = asConstPayload(payloadPtr)) {
		return bus->emit(eventName, *p);
	}
	return bus->emit(eventName);
}

BIRD_DART_EVENT_API void bird_events_emit_immediate(const char* eventName,
													const void* payloadPtr) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	if (!bus || !eventName || eventName[0] == '\0') return;
	if (const auto* p = asConstPayload(payloadPtr)) {
		bus->emitImmediate(eventName, *p);
	} else {
		bus->emitImmediate(eventName);
	}
}

BIRD_DART_EVENT_API uint64_t bird_events_subscribe(const char* eventName,
												   uint64_t ownerEntityId) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	auto& state = bridgeState();
	if (!bus || !eventName || eventName[0] == '\0' || !state.onDispatch) {
		return bird::kInvalidSubscriptionID;
	}

	ensureUnsubscribeHookInstalled(bus);

	auto idBox = std::make_shared<bird::SubscriptionID>(
		bird::kInvalidSubscriptionID);
	const bird::SubscriptionID id = bus->subscribe(
		eventName,
		[idBox](const bird::EventPayload& payload) {
			auto& st = bridgeState();
			if (!st.onDispatch || !st.isolate) return;
			ScopedIsolateEntry guard(st.isolate);
			st.onDispatch(*idBox, static_cast<const void*>(&payload));
		},
		ownerEntityId);

	if (id == bird::kInvalidSubscriptionID) {
		return bird::kInvalidSubscriptionID;
	}
	*idBox = id;
	state.dartSubscriptions.insert(id);
	return id;
}

BIRD_DART_EVENT_API bool bird_events_unsubscribe(uint64_t subscriptionId) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	if (!bus) return false;
	return bus->unsubscribe(subscriptionId);
}

BIRD_DART_EVENT_API uint64_t
bird_events_unsubscribe_by_owner(uint64_t ownerEntityId) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	if (!bus) return 0;
	return static_cast<uint64_t>(bus->unsubscribeByOwner(ownerEntityId));
}

BIRD_DART_EVENT_API uint64_t bird_events_dispatch_queued(void) {
	bird::EventBus* bus = bird::EventBus::getGlobal();
	if (!bus) return 0;
	return static_cast<uint64_t>(bus->dispatchQueued());
}

}  // extern "C"
