#ifndef BIRD_DART_EVENT_BRIDGE_H
#define BIRD_DART_EVENT_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32) || defined(_MSC_VER)
#define BIRD_DART_EVENT_API __declspec(dllexport)
#else
#define BIRD_DART_EVENT_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Dart NativeCallable signature invoked when an event fires for a Dart
 * subscription.
 *
 * @param subscriptionId SubscriptionID returned by bird_events_subscribe().
 * @param payloadPtr Borrowed pointer to a const bird::EventPayload valid only
 *                   for the duration of the callback.
 */
typedef void (*BirdDartEventDispatchFn)(uint64_t subscriptionId,
										const void* payloadPtr);

/**
 * Dart NativeCallable signature invoked when a Dart-owned subscription is
 * removed on the C++ side (manually via unsubscribe(), automatically via
 * WorldObject::deinit() / ~WorldObject(), or via EventBus::clear()).
 *
 * @param subscriptionId SubscriptionID that was removed.
 */
typedef void (*BirdDartEventUnsubscribeFn)(uint64_t subscriptionId);

/**
 * Registers the isolate-local Dart dispatch and unsubscribe trampolines with
 * the native event bridge and captures the current Dart_Isolate.
 *
 * Must be called from the Dart isolate before subscribing to events.
 */
BIRD_DART_EVENT_API void bird_events_register_callbacks(
	BirdDartEventDispatchFn onDispatch,
	BirdDartEventUnsubscribeFn onUnsubscribe);

/**
 * Unregisters the Dart event trampolines and detaches the native unsubscribe
 * hook. Safe to call multiple times or during VM shutdown.
 */
BIRD_DART_EVENT_API void bird_events_unregister_callbacks(void);

// ---------------------------------------------------------------------------
// EventPayload C ABI (builder & reader)
// ---------------------------------------------------------------------------

/** Allocates a new empty bird::EventPayload on the heap. */
BIRD_DART_EVENT_API void* bird_events_payload_new(void);

/** Frees a bird::EventPayload previously created by bird_events_payload_new. */
BIRD_DART_EVENT_API void bird_events_payload_delete(void* payloadPtr);

BIRD_DART_EVENT_API void bird_events_payload_set_null(void* payloadPtr,
													  const char* key);
BIRD_DART_EVENT_API void bird_events_payload_set_bool(void* payloadPtr,
													  const char* key,
													  bool value);
BIRD_DART_EVENT_API void bird_events_payload_set_int(void* payloadPtr,
													 const char* key,
													 int64_t value);
BIRD_DART_EVENT_API void bird_events_payload_set_double(void* payloadPtr,
														const char* key,
														double value);
BIRD_DART_EVENT_API void bird_events_payload_set_string(void* payloadPtr,
														const char* key,
														const char* value);
BIRD_DART_EVENT_API void bird_events_payload_set_vec2(void* payloadPtr,
													  const char* key, float x,
													  float y);
BIRD_DART_EVENT_API void bird_events_payload_set_vec3(void* payloadPtr,
													  const char* key, float x,
													  float y, float z);
BIRD_DART_EVENT_API void bird_events_payload_set_vec4(void* payloadPtr,
													  const char* key, float x,
													  float y, float z,
													  float w);

/** Returns the number of key-value fields in payloadPtr (0 if null). */
BIRD_DART_EVENT_API uint32_t
bird_events_payload_field_count(const void* payloadPtr);

/** Returns the null-terminated key name for field index (borrowed pointer). */
BIRD_DART_EVENT_API const char* bird_events_payload_field_key(
	const void* payloadPtr, uint32_t index);

/** Returns the EventValueType discriminator (uint8_t) for field index. */
BIRD_DART_EVENT_API uint8_t
bird_events_payload_field_type(const void* payloadPtr, uint32_t index);

BIRD_DART_EVENT_API bool bird_events_payload_field_bool(const void* payloadPtr,
														uint32_t index);
BIRD_DART_EVENT_API int64_t
bird_events_payload_field_int(const void* payloadPtr, uint32_t index);
BIRD_DART_EVENT_API double
bird_events_payload_field_double(const void* payloadPtr, uint32_t index);
BIRD_DART_EVENT_API const char* bird_events_payload_field_string(
	const void* payloadPtr, uint32_t index);
BIRD_DART_EVENT_API void bird_events_payload_field_vec2(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY);
BIRD_DART_EVENT_API void bird_events_payload_field_vec3(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY,
														float* outZ);
BIRD_DART_EVENT_API void bird_events_payload_field_vec4(const void* payloadPtr,
														uint32_t index,
														float* outX,
														float* outY,
														float* outZ,
														float* outW);

// ---------------------------------------------------------------------------
// EventBus C ABI
// ---------------------------------------------------------------------------

/**
 * Enqueues an event onto the global EVENT_BUS ring buffer.
 * @param eventName Null-terminated event name.
 * @param payloadPtr Optional pointer to bird::EventPayload (copied; may be NULL).
 * @return true if enqueued, false if EVENT_BUS is null, name is empty, or full.
 */
BIRD_DART_EVENT_API bool bird_events_emit(const char* eventName,
										  const void* payloadPtr);

/**
 * Synchronously dispatches an event on the global EVENT_BUS.
 * @param eventName Null-terminated event name.
 * @param payloadPtr Optional pointer to bird::EventPayload (may be NULL).
 */
BIRD_DART_EVENT_API void bird_events_emit_immediate(const char* eventName,
													const void* payloadPtr);

/**
 * Subscribes a Dart listener to eventName on the global EVENT_BUS.
 *
 * @param eventName Null-terminated event name.
 * @param ownerEntityId Owning WorldObject::getID(), or 0 for global scope.
 * @return Non-zero SubscriptionID on success, or 0 on failure.
 */
BIRD_DART_EVENT_API uint64_t bird_events_subscribe(const char* eventName,
												   uint64_t ownerEntityId);

/**
 * Cancels a subscription by handle on the global EVENT_BUS.
 * @return true if removed, false otherwise.
 */
BIRD_DART_EVENT_API bool bird_events_unsubscribe(uint64_t subscriptionId);

/**
 * Cancels all subscriptions owned by ownerEntityId on the global EVENT_BUS.
 * @return Number of subscriptions removed.
 */
BIRD_DART_EVENT_API uint64_t
bird_events_unsubscribe_by_owner(uint64_t ownerEntityId);

/**
 * Flushes all currently queued events on the global EVENT_BUS.
 * @return Number of events dispatched.
 */
BIRD_DART_EVENT_API uint64_t bird_events_dispatch_queued(void);

#ifdef __cplusplus
}
#endif

#endif	// BIRD_DART_EVENT_BRIDGE_H
