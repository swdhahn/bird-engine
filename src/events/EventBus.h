#ifndef BIRD_EVENTS_EVENTBUS_H
#define BIRD_EVENTS_EVENTBUS_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../util/Constants.h"
#include "EventPayload.h"
#include "EventTypes.h"

namespace bird {

/**
 * Callback signature invoked when an event is dispatched to a native listener.
 *
 * @param payload Read-only key-value payload attached to the event.
 */
using EventCallback = std::function<void(const EventPayload& payload)>;

/**
 * Optional hook invoked whenever a subscription is removed (manually via
 * unsubscribe(), automatically via unsubscribeByOwner(), or via clear()).
 * Used by scripting bridges (e.g. DartEventBridge) to release script-side
 * callback references without leaking memory.
 */
using UnsubscribeHook = std::function<void(SubscriptionID subscriptionId)>;

/**
 * Decoupled publish-subscribe Event & Messaging Bus shared across native C++
 * engine subsystems and Dart scripts.
 *
 * Supports two dispatch modes:
 * - **Queued events (default, emit())**: Pushed into a fixed-capacity ring
 *   buffer and dispatched in FIFO order when dispatchQueued() runs (called
 *   once per frame in Application::tick(), after physics and before UI and
 *   script process()). Events emitted by listeners during dispatchQueued()
 *   are deferred to the next frame's dispatch cycle, preventing infinite loops.
 * - **Immediate events (emitImmediate())**: Dispatched synchronously to all
 *   current subscribers before returning.
 *
 * Re-entrancy guarantees:
 * - Listeners may safely call subscribe(), unsubscribe(), unsubscribeByOwner(),
 *   emit(), or emitImmediate() from inside any event callback.
 * - Unsubscribing a listener (or destroying its owning entity) during dispatch
 *   takes effect immediately: that listener will not be invoked for remaining
 *   notifications in the current dispatch or any subsequent event.
 * - Subscriptions added during dispatch are deferred until the outermost
 *   dispatch completes so active listener iterators are never invalidated.
 *
 * Automatic lifecycle cleanup:
 * - Pass a WorldObject's EntityID (object->getID()) as ownerEntityId when
 *   subscribing. WorldObject::deinit() and WorldObject::~WorldObject()
 *   automatically call unsubscribeByOwner(m_id), ensuring dead entities never
 *   receive stale callbacks.
 */
class EventBus {
   public:
	/** Default fixed capacity of the queued-event ring buffer. */
	static constexpr size_t kDefaultQueueCapacity = 1024;

	/**
	 * Constructs an EventBus with a ring buffer of the requested capacity.
	 *
	 * @param queueCapacity Maximum number of queued events held between
	 *                      dispatchQueued() calls (clamped to at least 1).
	 */
	explicit EventBus(size_t queueCapacity = kDefaultQueueCapacity);
	~EventBus() = default;

	EventBus(const EventBus&) = delete;
	EventBus& operator=(const EventBus&) = delete;

	/**
	 * Subscribes a callback to events identified by string name.
	 *
	 * @param eventName Dot-namespaced event name (e.g. "player.damaged").
	 * @param callback Function invoked when the event fires; must be non-empty.
	 * @param ownerEntityId Optional owning WorldObject::getID() for automatic
	 *                      cleanup on entity deinit/destruction
	 *                      (kNoOwnerEntityID for global subscriptions).
	 * @return Unique SubscriptionID, or kInvalidSubscriptionID if eventName is
	 *         empty or callback is empty.
	 */
	SubscriptionID subscribe(const std::string& eventName,
							 EventCallback callback,
							 EntityID ownerEntityId = kNoOwnerEntityID);

	/**
	 * Subscribes a callback to events identified by a pre-hashed EventTypeID.
	 *
	 * @param eventId Non-zero FNV-1a hash from hashEventName().
	 * @param callback Function invoked when the event fires; must be non-empty.
	 * @param ownerEntityId Optional owning WorldObject::getID() for automatic
	 *                      cleanup on entity deinit/destruction.
	 * @return Unique SubscriptionID, or kInvalidSubscriptionID if eventId is
	 *         kInvalidEventTypeID or callback is empty.
	 */
	SubscriptionID subscribe(EventTypeID eventId, EventCallback callback,
							 EntityID ownerEntityId = kNoOwnerEntityID);

	/**
	 * Cancels a single subscription by handle. Safe to call during dispatch,
	 * including from within the listener being unsubscribed.
	 *
	 * @param subscriptionId Handle previously returned by subscribe().
	 * @return true if the subscription existed and was removed; false otherwise.
	 */
	bool unsubscribe(SubscriptionID subscriptionId);

	/**
	 * Cancels all subscriptions owned by the specified entity. Called
	 * automatically by WorldObject::deinit() and WorldObject::~WorldObject().
	 * Safe to call during event dispatch.
	 *
	 * @param ownerEntityId EntityID whose subscriptions should be removed.
	 * @return Number of subscriptions removed.
	 */
	size_t unsubscribeByOwner(EntityID ownerEntityId);

	/**
	 * Enqueues an event by string name into the ring buffer for deferred
	 * dispatch during the next dispatchQueued() call.
	 *
	 * @param eventName Event name (hashed via hashEventName()).
	 * @param payload Optional key-value payload moved into the queue.
	 * @return true if enqueued; false if eventName is empty or the ring buffer
	 *         is full (in which case droppedCount() is incremented).
	 */
	bool emit(const std::string& eventName, EventPayload payload = {});

	/**
	 * Enqueues an event by pre-hashed EventTypeID into the ring buffer for
	 * deferred dispatch during the next dispatchQueued() call.
	 *
	 * @param eventId Non-zero FNV-1a hash from hashEventName().
	 * @param payload Optional key-value payload moved into the queue.
	 * @return true if enqueued; false if eventId is invalid or the ring buffer
	 *         is full.
	 */
	bool emit(EventTypeID eventId, EventPayload payload = {});

	/**
	 * Synchronously dispatches an event by string name to all currently active
	 * subscribers before returning.
	 *
	 * @param eventName Event name (hashed via hashEventName()).
	 * @param payload Optional read-only payload passed to every listener.
	 */
	void emitImmediate(const std::string& eventName,
					   const EventPayload& payload = {});

	/**
	 * Synchronously dispatches an event by pre-hashed EventTypeID to all
	 * currently active subscribers before returning.
	 *
	 * @param eventId Non-zero FNV-1a hash from hashEventName().
	 * @param payload Optional read-only payload passed to every listener.
	 */
	void emitImmediate(EventTypeID eventId, const EventPayload& payload = {});

	/**
	 * Drains and dispatches all events currently sitting in the ring buffer
	 * in FIFO order.
	 *
	 * Only events queued prior to this call are dispatched; any new events
	 * emitted via emit() by listeners during dispatch remain queued for the
	 * next call to dispatchQueued().
	 *
	 * @return Number of queued events dispatched during this call.
	 */
	size_t dispatchQueued();

	/**
	 * Returns true if at least one active (or pending-add) subscriber is
	 * registered for the given event name.
	 *
	 * Native engine systems can use this as a fast O(1) guard before
	 * constructing an EventPayload.
	 *
	 * @param eventName Event name to query.
	 */
	bool hasSubscribers(const std::string& eventName) const;

	/**
	 * Returns true if at least one active (or pending-add) subscriber is
	 * registered for the given EventTypeID.
	 *
	 * @param eventId Pre-hashed EventTypeID to query.
	 */
	bool hasSubscribers(EventTypeID eventId) const;

	/**
	 * Returns the number of active subscriptions for a specific EventTypeID.
	 */
	size_t subscriberCount(EventTypeID eventId) const;

	/**
	 * Returns the total number of active subscriptions across all events.
	 */
	size_t totalSubscriberCount() const;

	/**
	 * Returns the number of events currently waiting in the ring buffer.
	 */
	size_t queuedCount() const { return m_queueCount; }

	/**
	 * Returns the maximum capacity of the queued-event ring buffer.
	 */
	size_t queueCapacity() const { return m_queue.size(); }

	/**
	 * Returns the cumulative number of queued events dropped due to ring
	 * buffer overflow since construction or the last clear().
	 */
	uint64_t droppedCount() const { return m_droppedCount; }

	/**
	 * Removes all queued events and all subscriptions (invoking the
	 * unsubscribe hook for each active subscription), and resets droppedCount.
	 */
	void clear();

	/**
	 * Registers (or clears with nullptr) a notification hook called whenever
	 * any subscription is removed. Intended for scripting language bridges.
	 */
	BIRD_NO_EXPORT void setUnsubscribeHook(UnsubscribeHook hook);

	/**
	 * Returns the engine-wide global EventBus instance (same as EVENT_BUS,
	 * safe across shared-library boundaries with -Bsymbolic).
	 */
	static EventBus* getGlobal();

	/**
	 * Sets the engine-wide global EventBus instance (updates EVENT_BUS inside
	 * Bird_core). Intended for Application startup/teardown and unit tests.
	 */
	static void setGlobal(EventBus* bus);

   private:
	struct SubscriptionEntry {
		SubscriptionID id = kInvalidSubscriptionID;
		EventTypeID eventId = kInvalidEventTypeID;
		EntityID ownerId = kNoOwnerEntityID;
		EventCallback callback;
		bool pendingRemoval = false;
	};

	struct SubscriptionMeta {
		EventTypeID eventId = kInvalidEventTypeID;
		EntityID ownerId = kNoOwnerEntityID;
	};

	struct QueuedEvent {
		EventTypeID eventId = kInvalidEventTypeID;
		EventPayload payload;
	};

	void dispatchSingle(EventTypeID eventId, const EventPayload& payload);
	void flushPendingMutations();

	std::vector<QueuedEvent> m_queue;
	size_t m_queueHead = 0;
	size_t m_queueTail = 0;
	size_t m_queueCount = 0;
	uint64_t m_droppedCount = 0;

	SubscriptionID m_nextSubscriptionId = 0;
	uint32_t m_dispatchDepth = 0;
	bool m_needsCompaction = false;

	std::unordered_map<EventTypeID, std::vector<SubscriptionEntry>> m_listeners;
	std::vector<SubscriptionEntry> m_pendingAdds;
	std::unordered_map<SubscriptionID, SubscriptionMeta> m_subscriptionMeta;
	std::unordered_map<EntityID, std::vector<SubscriptionID>> m_ownerIndex;
	UnsubscribeHook m_unsubscribeHook;
};

}  // namespace bird

#endif	// BIRD_EVENTS_EVENTBUS_H
