#include "EventBus.h"

#include <algorithm>
#include <utility>

#include "../platform/Log.h"
#include "../scene/WorldObject.h"

namespace bird {

namespace {

struct DispatchDepthGuard {
	uint32_t& depth;
	std::function<void()> onExitZero;

	DispatchDepthGuard(uint32_t& d, std::function<void()> flushFn)
		: depth(d), onExitZero(std::move(flushFn)) {
		++depth;
	}

	~DispatchDepthGuard() {
		--depth;
		if (depth == 0 && onExitZero) {
			onExitZero();
		}
	}
};

}  // namespace

EventBus::EventBus(size_t queueCapacity)
	: m_queue(queueCapacity > 0 ? queueCapacity : 1) {}

SubscriptionID EventBus::subscribe(std::string_view eventName,
								   EventCallback callback,
								   EntityID ownerEntityId) {
	return subscribe(hashEventName(eventName), std::move(callback),
					 ownerEntityId);
}

SubscriptionID EventBus::subscribe(EventTypeID eventId, EventCallback callback,
								   EntityID ownerEntityId) {
	if (eventId == kInvalidEventTypeID || !callback) {
		return kInvalidSubscriptionID;
	}

	const SubscriptionID id = ++m_nextSubscriptionId;
	SubscriptionEntry entry;
	entry.id = id;
	entry.eventId = eventId;
	entry.ownerId = ownerEntityId;
	entry.callback = std::move(callback);
	entry.pendingRemoval = false;

	m_subscriptionMeta.emplace(id, SubscriptionMeta{eventId, ownerEntityId});
	if (ownerEntityId != kNoOwnerEntityID) {
		m_ownerIndex[ownerEntityId].push_back(id);
	}

	if (m_dispatchDepth > 0) {
		m_pendingAdds.push_back(std::move(entry));
	} else {
		m_listeners[eventId].push_back(std::move(entry));
	}

	return id;
}

bool EventBus::unsubscribe(SubscriptionID subscriptionId) {
	if (subscriptionId == kInvalidSubscriptionID) return false;

	auto metaIt = m_subscriptionMeta.find(subscriptionId);
	if (metaIt == m_subscriptionMeta.end()) return false;

	const EventTypeID eventId = metaIt->second.eventId;
	const EntityID ownerId = metaIt->second.ownerId;
	m_subscriptionMeta.erase(metaIt);

	// Remove from owner index if owned by an entity.
	if (ownerId != kNoOwnerEntityID) {
		auto ownerIt = m_ownerIndex.find(ownerId);
		if (ownerIt != m_ownerIndex.end()) {
			auto& ids = ownerIt->second;
			ids.erase(std::remove(ids.begin(), ids.end(), subscriptionId),
					  ids.end());
			if (ids.empty()) {
				m_ownerIndex.erase(ownerIt);
			}
		}
	}

	// Check pending additions first (subscribed & unsubscribed within same dispatch).
	bool removedFromPending = false;
	for (auto it = m_pendingAdds.begin(); it != m_pendingAdds.end(); ++it) {
		if (it->id == subscriptionId) {
			m_pendingAdds.erase(it);
			removedFromPending = true;
			break;
		}
	}

	if (!removedFromPending) {
		auto bucketIt = m_listeners.find(eventId);
		if (bucketIt != m_listeners.end()) {
			auto& bucket = bucketIt->second;
			if (m_dispatchDepth > 0) {
				for (auto& entry : bucket) {
					if (entry.id == subscriptionId) {
						entry.pendingRemoval = true;
						entry.callback = nullptr;
						m_needsCompaction = true;
						break;
					}
				}
			} else {
				bucket.erase(
					std::remove_if(bucket.begin(), bucket.end(),
								   [subscriptionId](const SubscriptionEntry& e) {
									   return e.id == subscriptionId;
								   }),
					bucket.end());
				if (bucket.empty()) {
					m_listeners.erase(bucketIt);
				}
			}
		}
	}

	if (m_unsubscribeHook) {
		m_unsubscribeHook(subscriptionId);
	}
	return true;
}

size_t EventBus::unsubscribeByOwner(EntityID ownerEntityId) {
	if (ownerEntityId == kNoOwnerEntityID) return 0;

	auto ownerIt = m_ownerIndex.find(ownerEntityId);
	if (ownerIt == m_ownerIndex.end()) return 0;

	// Move IDs out so unsubscribe() doesn't invalidate iteration.
	std::vector<SubscriptionID> ids = std::move(ownerIt->second);
	m_ownerIndex.erase(ownerIt);

	size_t removed = 0;
	for (SubscriptionID id : ids) {
		if (unsubscribe(id)) {
			++removed;
		}
	}
	return removed;
}

bool EventBus::emit(std::string_view eventName, EventPayload payload) {
	return emit(hashEventName(eventName), std::move(payload));
}

bool EventBus::emit(EventTypeID eventId, EventPayload payload) {
	if (eventId == kInvalidEventTypeID) return false;

	if (m_queueCount >= m_queue.size()) {
		++m_droppedCount;
		platform::log(platform::LogLevel::Warning,
					  "EventBus: ring buffer full (capacity " +
						  std::to_string(m_queue.size()) +
						  "); dropping queued event.");
		return false;
	}

	m_queue[m_queueTail].eventId = eventId;
	m_queue[m_queueTail].payload = std::move(payload);
	m_queueTail = (m_queueTail + 1) % m_queue.size();
	++m_queueCount;
	return true;
}

void EventBus::emitImmediate(std::string_view eventName,
							 const EventPayload& payload) {
	emitImmediate(hashEventName(eventName), payload);
}

void EventBus::emitImmediate(EventTypeID eventId, const EventPayload& payload) {
	if (eventId == kInvalidEventTypeID) return;
	dispatchSingle(eventId, payload);
}

size_t EventBus::dispatchQueued() {
	// Snapshot the count so events emitted during this pass wait until next frame.
	const size_t toDispatch = m_queueCount;
	for (size_t i = 0; i < toDispatch; ++i) {
		QueuedEvent ev = std::move(m_queue[m_queueHead]);
		m_queue[m_queueHead] = QueuedEvent{};
		m_queueHead = (m_queueHead + 1) % m_queue.size();
		--m_queueCount;

		dispatchSingle(ev.eventId, ev.payload);
	}
	return toDispatch;
}

void EventBus::dispatchSingle(EventTypeID eventId,
							  const EventPayload& payload) {
	auto bucketIt = m_listeners.find(eventId);
	if (bucketIt == m_listeners.end() || bucketIt->second.empty()) {
		return;
	}

	DispatchDepthGuard guard(m_dispatchDepth,
							 [this]() { flushPendingMutations(); });

	// Because m_dispatchDepth > 0, m_listeners will not be rehashed or mutated
	// structurally until the outermost dispatch completes.
	auto& bucket = bucketIt->second;
	const size_t count = bucket.size();
	for (size_t i = 0; i < count; ++i) {
		SubscriptionEntry& entry = bucket[i];
		if (entry.pendingRemoval || !entry.callback) continue;
		// Copy callback handle locally in case the callback unsubscribes itself
		// and clears entry.callback mid-invocation.
		EventCallback cb = entry.callback;
		cb(payload);
	}
}

void EventBus::flushPendingMutations() {
	if (m_needsCompaction) {
		for (auto it = m_listeners.begin(); it != m_listeners.end();) {
			auto& bucket = it->second;
			bucket.erase(
				std::remove_if(bucket.begin(), bucket.end(),
							   [](const SubscriptionEntry& e) {
								   return e.pendingRemoval;
							   }),
				bucket.end());
			if (bucket.empty()) {
				it = m_listeners.erase(it);
			} else {
				++it;
			}
		}
		m_needsCompaction = false;
	}

	if (!m_pendingAdds.empty()) {
		std::vector<SubscriptionEntry> adds = std::move(m_pendingAdds);
		m_pendingAdds.clear();
		for (auto& entry : adds) {
			if (!entry.pendingRemoval && entry.callback) {
				m_listeners[entry.eventId].push_back(std::move(entry));
			}
		}
	}
}

bool EventBus::hasSubscribers(std::string_view eventName) const {
	return hasSubscribers(hashEventName(eventName));
}

bool EventBus::hasSubscribers(EventTypeID eventId) const {
	if (eventId == kInvalidEventTypeID) return false;

	auto it = m_listeners.find(eventId);
	if (it != m_listeners.end()) {
		for (const auto& entry : it->second) {
			if (!entry.pendingRemoval) return true;
		}
	}
	for (const auto& entry : m_pendingAdds) {
		if (entry.eventId == eventId && !entry.pendingRemoval) return true;
	}
	return false;
}

size_t EventBus::subscriberCount(EventTypeID eventId) const {
	if (eventId == kInvalidEventTypeID) return 0;

	size_t count = 0;
	auto it = m_listeners.find(eventId);
	if (it != m_listeners.end()) {
		for (const auto& entry : it->second) {
			if (!entry.pendingRemoval) ++count;
		}
	}
	for (const auto& entry : m_pendingAdds) {
		if (entry.eventId == eventId && !entry.pendingRemoval) ++count;
	}
	return count;
}

size_t EventBus::totalSubscriberCount() const {
	return m_subscriptionMeta.size();
}

void EventBus::clear() {
	std::vector<SubscriptionID> removedIds;
	if (m_unsubscribeHook) {
		removedIds.reserve(m_subscriptionMeta.size());
		for (const auto& [id, meta] : m_subscriptionMeta) {
			removedIds.push_back(id);
		}
	}

	m_listeners.clear();
	m_pendingAdds.clear();
	m_subscriptionMeta.clear();
	m_ownerIndex.clear();
	m_needsCompaction = false;

	for (auto& slot : m_queue) {
		slot = QueuedEvent{};
	}
	m_queueHead = 0;
	m_queueTail = 0;
	m_queueCount = 0;
	m_droppedCount = 0;

	if (m_unsubscribeHook) {
		for (SubscriptionID id : removedIds) {
			m_unsubscribeHook(id);
		}
	}
}

void EventBus::setUnsubscribeHook(UnsubscribeHook hook) {
	m_unsubscribeHook = std::move(hook);
}

EventBus* EventBus::getGlobal() {
	return EVENT_BUS;
}

void EventBus::setGlobal(EventBus* bus) {
	EVENT_BUS = bus;
}

}  // namespace bird
