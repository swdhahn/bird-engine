#ifndef BIRD_EVENTS_EVENTTYPES_H
#define BIRD_EVENTS_EVENTTYPES_H

#include <cstdint>
#include <string_view>

namespace bird {

/**
 * Hashed 64-bit identifier derived from an event's string name via FNV-1a.
 *
 * Used internally by EventBus for O(1) listener bucket lookups without
 * allocating strings on hot paths. Compute at compile time with
 * hashEventName("my.event") or pass a string name directly to EventBus.
 */
using EventTypeID = uint64_t;

/**
 * Unique handle returned by EventBus::subscribe() to identify a single
 * listener registration. Pass this handle to EventBus::unsubscribe() to
 * manually cancel a subscription.
 */
using SubscriptionID = uint64_t;

/**
 * Entity ownership identifier corresponding to WorldObject::getID().
 *
 * Subscriptions registered with a non-zero EntityID are automatically
 * unsubscribed when that WorldObject's deinit() runs or when the entity is
 * destroyed.
 */
using EntityID = uint64_t;

/** Sentinel returned when a subscription fails or is not registered. */
constexpr SubscriptionID kInvalidSubscriptionID = 0;

/** Sentinel indicating a global subscription not owned by any WorldObject. */
constexpr EntityID kNoOwnerEntityID = 0;

/** Sentinel indicating an empty or invalid event type identifier. */
constexpr EventTypeID kInvalidEventTypeID = 0;

/**
 * Computes a deterministic 64-bit FNV-1a hash of a dot-namespaced event name.
 *
 * Marked constexpr so C++ subsystems can precompute EventTypeID constants at
 * compile time while remaining identical to dynamic runtime hashes from
 * C++ or Dart scripts.
 *
 * @param name Event name (e.g. "player.damaged", "physics.contact_begin").
 * @return Non-zero 64-bit EventTypeID, or kInvalidEventTypeID if name is empty.
 */
constexpr EventTypeID hashEventName(std::string_view name) noexcept {
	if (name.empty()) return kInvalidEventTypeID;
	constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
	constexpr uint64_t kFnvPrime = 1099511628211ULL;
	uint64_t hash = kFnvOffsetBasis;
	for (const char ch : name) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(ch));
		hash *= kFnvPrime;
	}
	// Reserve 0 exclusively for kInvalidEventTypeID.
	return hash == kInvalidEventTypeID ? 1ULL : hash;
}

}  // namespace bird

#endif	// BIRD_EVENTS_EVENTTYPES_H
