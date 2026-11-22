#ifndef BIRD_EVENTS_EVENTPAYLOAD_H
#define BIRD_EVENTS_EVENTPAYLOAD_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "../util/Constants.h"
#include "../util/Math.h"

namespace bird {

/**
 * Discriminator enum identifying the active runtime type stored in an
 * EventPayload field.
 *
 * Values are part of the C ABI shared with the Dart event bridge; do not
 * reorder existing enumerators.
 */
enum class EventValueType : uint8_t {
	Null = 0,
	Bool = 1,
	Int = 2,
	Double = 3,
	String = 4,
	Vector2 = 5,
	Vector3 = 6,
	Vector4 = 7,
};

/**
 * Variant value type carried by a single key in an EventPayload.
 */
using EventValue =
	std::variant<std::monostate, bool, int64_t, double, std::string, Vector2,
				 Vector3, Vector4>;

/**
 * Single key-value entry stored inside an EventPayload.
 */
struct EventField {
	/** Field name (e.g. "damage", "entityId", "hitPoint"). */
	std::string key;
	/** Typed value associated with key. */
	EventValue value;
};

/**
 * Lightweight, flexible key-value payload passed with events across both
 * native C++ subsystems and Dart scripts.
 *
 * Internally backed by a compact flat vector of EventField entries rather than
 * a hash map: empty payloads perform zero heap allocations, typical game events
 * (1-6 fields) remain cache-friendly, field insertion order is preserved, and
 * indexed inspection enables fast zero-copy marshalling across the Dart FFI
 * boundary.
 *
 * Usage example (C++):
 * @code
 * bird::EventPayload payload;
 * payload.set("damage", 25)
 *        .set("critical", true)
 *        .set("hitPoint", bird::Vector3(1.0f, 2.0f, 3.0f));
 * bird::EVENT_BUS->emit("player.damaged", payload);
 * @endcode
 */
class EventPayload {
   public:
	EventPayload() = default;
	~EventPayload() = default;

	/**
	 * Sets a boolean field. Overwrites any existing field with the same key.
	 * @param key Field name.
	 * @param value Boolean value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, bool value);

	/**
	 * Sets a 64-bit signed integer field.
	 * @param key Field name.
	 * @param value Integer value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, int64_t value);

	/**
	 * Convenience overload storing a 32-bit int as int64_t.
	 * @param key Field name.
	 * @param value Integer value.
	 * @return Reference to *this for fluent chaining.
	 */
	BIRD_NO_EXPORT EventPayload& set(std::string_view key, int value);

	/**
	 * Convenience overload storing an unsigned 64-bit ID (such as EntityID) as
	 * int64_t.
	 * @param key Field name.
	 * @param value Unsigned 64-bit value.
	 * @return Reference to *this for fluent chaining.
	 */
	BIRD_NO_EXPORT EventPayload& set(std::string_view key, uint64_t value);

	/**
	 * Sets a double-precision floating-point field.
	 * @param key Field name.
	 * @param value Floating-point value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, double value);

	/**
	 * Convenience overload storing a float as double.
	 * @param key Field name.
	 * @param value Floating-point value.
	 * @return Reference to *this for fluent chaining.
	 */
	BIRD_NO_EXPORT EventPayload& set(std::string_view key, float value);

	/**
	 * Sets a string field.
	 * @param key Field name.
	 * @param value String value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, std::string value);

	/**
	 * Convenience overload accepting a C-string literal without implicit bool
	 * conversion.
	 * @param key Field name.
	 * @param value Null-terminated C string (treated as empty if nullptr).
	 * @return Reference to *this for fluent chaining.
	 */
	BIRD_NO_EXPORT EventPayload& set(std::string_view key, const char* value);

	/**
	 * Sets a 2D vector field.
	 * @param key Field name.
	 * @param value Vector2 value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, const Vector2& value);

	/**
	 * Sets a 3D vector field.
	 * @param key Field name.
	 * @param value Vector3 value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, const Vector3& value);

	/**
	 * Sets a 4D vector field.
	 * @param key Field name.
	 * @param value Vector4 value.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& set(std::string_view key, const Vector4& value);

	/**
	 * Sets a field explicitly to Null (std::monostate).
	 * @param key Field name.
	 * @return Reference to *this for fluent chaining.
	 */
	EventPayload& setNull(std::string_view key);

	/**
	 * Returns true if a field with the given key exists in this payload.
	 * @param key Field name to look up.
	 */
	bool has(std::string_view key) const;

	/**
	 * Returns the stored EventValueType for key, or EventValueType::Null if
	 * absent.
	 * @param key Field name to inspect.
	 */
	EventValueType getType(std::string_view key) const;

	/**
	 * Retrieves a boolean field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	bool getBool(std::string_view key, bool defaultValue = false) const;

	/**
	 * Retrieves an integer field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	int64_t getInt(std::string_view key, int64_t defaultValue = 0) const;

	/**
	 * Retrieves a double field (also widens int64_t fields if present), or
	 * defaultValue if missing or incompatible.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	double getDouble(std::string_view key, double defaultValue = 0.0) const;

	/**
	 * Retrieves a single-precision float field (widening int64_t if present).
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	float getFloat(std::string_view key, float defaultValue = 0.0f) const;

	/**
	 * Retrieves a string field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	std::string getString(std::string_view key,
						  const std::string& defaultValue = "") const;

	/**
	 * Retrieves a Vector2 field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	Vector2 getVector2(std::string_view key,
					   const Vector2& defaultValue = Vector2(0.0f)) const;

	/**
	 * Retrieves a Vector3 field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	Vector3 getVector3(std::string_view key,
					   const Vector3& defaultValue = Vector3(0.0f)) const;

	/**
	 * Retrieves a Vector4 field, or defaultValue if missing or of another type.
	 * @param key Field name.
	 * @param defaultValue Fallback value when absent or mismatched.
	 */
	Vector4 getVector4(std::string_view key,
					   const Vector4& defaultValue = Vector4(0.0f)) const;

	/** Removes all fields from this payload. */
	void clear();

	/** Returns true when no fields are stored. */
	bool isEmpty() const { return m_fields.empty(); }

	/** Returns the number of stored key-value fields. */
	size_t size() const { return m_fields.size(); }

	/**
	 * Returns direct read-only access to all fields in insertion order.
	 * Used by serialization and the Dart FFI bridge.
	 */
	BIRD_NO_EXPORT const std::vector<EventField>& fields() const {
		return m_fields;
	}

	/**
	 * Returns the EventValueType discriminator for a given EventValue variant.
	 */
	BIRD_NO_EXPORT static EventValueType valueTypeOf(const EventValue& value);

   private:
	EventField* findField(std::string_view key);
	const EventField* findField(std::string_view key) const;
	EventPayload& setField(std::string_view key, EventValue value);

	std::vector<EventField> m_fields;
};

}  // namespace bird

#endif	// BIRD_EVENTS_EVENTPAYLOAD_H
