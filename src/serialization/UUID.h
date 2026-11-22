#ifndef BIRD_SERIALIZATION_UUID_H
#define BIRD_SERIALIZATION_UUID_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "../util/Constants.h"

namespace bird {

/**
 * 128-bit RFC 4122 version 4 universally unique identifier.
 *
 * Used to give every WorldObject a stable, persistent identity across scene
 * saves, scene loads, and prefab definitions (distinct from the sequential
 * runtime WorldObject::getID() counter).
 *
 * Canonical string format is lowercase hyphenated hex:
 * `xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx` (36 characters).
 */
class UUID {
   public:
	/** Constructs a nil (all-zero) UUID. */
	constexpr UUID() noexcept : m_high(0), m_low(0) {}

	/**
	 * Constructs a UUID from explicit 64-bit upper and lower words.
	 * @param high Most-significant 64 bits.
	 * @param low Least-significant 64 bits.
	 */
	constexpr UUID(uint64_t high, uint64_t low) noexcept
		: m_high(high), m_low(low) {}

	/**
	 * Generates a new random RFC 4122 version 4 UUID using a thread-local
	 * 64-bit Mersenne Twister seeded from system entropy.
	 * @return A newly generated non-nil UUID.
	 */
	static UUID generate();

	/** Returns the nil (all-zero) UUID. */
	static constexpr UUID nil() noexcept { return UUID(0, 0); }

	/**
	 * Parses a canonical 36-character hyphenated UUID string.
	 * Accepts both uppercase and lowercase hexadecimal digits.
	 *
	 * @param str Canonical UUID string (e.g. "550e8400-e29b-41d4-a716-446655440000").
	 * @return Parsed UUID, or std::nullopt if the string is malformed.
	 */
	BIRD_NO_EXPORT static std::optional<UUID> tryParse(const std::string& str);

	/**
	 * Parses a canonical UUID string, throwing std::invalid_argument on failure.
	 *
	 * @param str Canonical 36-character UUID string.
	 * @return Parsed UUID.
	 * @throws std::invalid_argument when str is not a valid UUID.
	 */
	static UUID fromString(const std::string& str);

	/**
	 * Formats this UUID as a lowercase 36-character canonical string.
	 * @return String of the form "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx".
	 */
	std::string toString() const;

	/** Returns true when both 64-bit words are zero. */
	constexpr bool isNil() const noexcept { return m_high == 0 && m_low == 0; }

	/** Returns the most-significant 64 bits. */
	constexpr uint64_t high() const noexcept { return m_high; }

	/** Returns the least-significant 64 bits. */
	constexpr uint64_t low() const noexcept { return m_low; }

	constexpr bool operator==(const UUID& other) const noexcept {
		return m_high == other.m_high && m_low == other.m_low;
	}

	constexpr bool operator!=(const UUID& other) const noexcept {
		return !(*this == other);
	}

	constexpr bool operator<(const UUID& other) const noexcept {
		return (m_high < other.m_high) ||
			   (m_high == other.m_high && m_low < other.m_low);
	}

   private:
	uint64_t m_high;
	uint64_t m_low;
};

/**
 * Strongly-typed reference to another entity by its stable UUID.
 *
 * Components that reference another WorldObject (e.g. a follow target,
 * joint anchor, or trigger activator) should store an EntityRef rather than
 * a raw pointer or raw UUID. During prefab instantiation, the serialization
 * system automatically remaps EntityRef fields whose UUID belongs to the
 * prefab's local UUID table to the newly spawned runtime entity's UUID.
 */
struct EntityRef {
	/** Stable UUID of the referenced entity (nil if unbound). */
	UUID uuid;

	constexpr EntityRef() noexcept = default;
	constexpr explicit EntityRef(UUID id) noexcept : uuid(id) {}

	/** Returns true when this reference does not point to any entity. */
	constexpr bool isNull() const noexcept { return uuid.isNil(); }

	constexpr bool operator==(const EntityRef& other) const noexcept {
		return uuid == other.uuid;
	}

	constexpr bool operator!=(const EntityRef& other) const noexcept {
		return uuid != other.uuid;
	}
};

}  // namespace bird

namespace std {

template <>
struct hash<bird::UUID> {
	size_t operator()(const bird::UUID& id) const noexcept {
		// SplitMix64-style combine of the two 64-bit halves.
		uint64_t h = id.high() ^ (id.low() + 0x9e3779b97f4a7c15ULL +
								  (id.high() << 6) + (id.high() >> 2));
		return static_cast<size_t>(h);
	}
};

}  // namespace std

#endif	// BIRD_SERIALIZATION_UUID_H
