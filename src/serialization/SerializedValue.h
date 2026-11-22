#ifndef BIRD_SERIALIZATION_SERIALIZED_VALUE_H
#define BIRD_SERIALIZATION_SERIALIZED_VALUE_H

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "../util/Math.h"
#include "UUID.h"

namespace bird {

/**
 * Discriminator identifying the active type stored inside a SerializedValue.
 */
enum class SerializedValueType : uint8_t {
	Null = 0,
	Bool,
	Int,
	Double,
	String,
	Vec2,
	Vec3,
	Vec4,
	Quat,
	Uuid,
	EntityRef,
	Array
};

/**
 * Dynamically-typed serializable property value.
 *
 * Used to represent exposed Dart script properties, prefab PropertyOverride
 * deltas, and type-erased reflected component field values. Supports all
 * primitive scalar types, strings, Bird Engine math types (Vector2/3/4,
 * Quaternion), UUIDs, EntityRef (which participate in prefab reference
 * remapping), and nested arrays.
 */
class SerializedValue {
   public:
	using Array = std::vector<SerializedValue>;
	using Storage = std::variant<std::monostate, bool, int64_t, double,
								 std::string, Vector2, Vector3, Vector4,
								 Quaternion, UUID, EntityRef, Array>;

	/** Constructs a Null SerializedValue. */
	SerializedValue() = default;

	SerializedValue(std::nullptr_t) : m_data(std::monostate{}) {}
	SerializedValue(bool v) : m_data(v) {}
	SerializedValue(int32_t v) : m_data(static_cast<int64_t>(v)) {}
	SerializedValue(uint32_t v) : m_data(static_cast<int64_t>(v)) {}
	SerializedValue(int64_t v) : m_data(v) {}
	SerializedValue(uint64_t v) : m_data(static_cast<int64_t>(v)) {}
	SerializedValue(float v) : m_data(static_cast<double>(v)) {}
	SerializedValue(double v) : m_data(v) {}
	SerializedValue(const char* v) : m_data(std::string(v ? v : "")) {}
	SerializedValue(std::string v) : m_data(std::move(v)) {}
	SerializedValue(const Vector2& v) : m_data(v) {}
	SerializedValue(const Vector3& v) : m_data(v) {}
	SerializedValue(const Vector4& v) : m_data(v) {}
	SerializedValue(const Quaternion& v) : m_data(v) {}
	SerializedValue(const UUID& v) : m_data(v) {}
	SerializedValue(const EntityRef& v) : m_data(v) {}
	SerializedValue(Array v) : m_data(std::move(v)) {}

	/** Returns the active SerializedValueType discriminator. */
	SerializedValueType type() const noexcept;

	bool isNull() const noexcept { return type() == SerializedValueType::Null; }
	bool isBool() const noexcept { return type() == SerializedValueType::Bool; }
	bool isInt() const noexcept { return type() == SerializedValueType::Int; }
	bool isDouble() const noexcept { return type() == SerializedValueType::Double; }
	bool isNumber() const noexcept { return isInt() || isDouble(); }
	bool isString() const noexcept { return type() == SerializedValueType::String; }
	bool isVec2() const noexcept { return type() == SerializedValueType::Vec2; }
	bool isVec3() const noexcept { return type() == SerializedValueType::Vec3; }
	bool isVec4() const noexcept { return type() == SerializedValueType::Vec4; }
	bool isQuat() const noexcept { return type() == SerializedValueType::Quat; }
	bool isUuid() const noexcept { return type() == SerializedValueType::Uuid; }
	bool isEntityRef() const noexcept { return type() == SerializedValueType::EntityRef; }
	bool isArray() const noexcept { return type() == SerializedValueType::Array; }

	/** Typed getters; numeric getters coerce between Int and Double seamlessly. */
	bool asBool(bool fallback = false) const;
	int64_t asInt(int64_t fallback = 0) const;
	double asDouble(double fallback = 0.0) const;
	float asFloat(float fallback = 0.0f) const;
	const std::string& asString() const;
	Vector2 asVec2(const Vector2& fallback = Vector2(0.0f)) const;
	Vector3 asVec3(const Vector3& fallback = Vector3(0.0f)) const;
	Vector4 asVec4(const Vector4& fallback = Vector4(0.0f)) const;
	Quaternion asQuat(const Quaternion& fallback = Quaternion(1.0f, 0.0f, 0.0f, 0.0f)) const;
	UUID asUuid(UUID fallback = UUID::nil()) const;
	EntityRef asEntityRef(EntityRef fallback = EntityRef{}) const;
	const Array& asArray() const;

	/** Returns mutable reference to the underlying std::variant storage. */
	Storage& storage() noexcept { return m_data; }
	const Storage& storage() const noexcept { return m_data; }

	/**
	 * Compares two values for semantic equality, using a small epsilon for
	 * floating-point scalars, vectors, and quaternions so prefab override
	 * diffing ignores harmless float-text rounding noise.
	 *
	 * @param other Value to compare against.
	 * @param epsilon Tolerance used for floating-point comparisons.
	 * @return True if both values are equivalent within epsilon.
	 */
	bool approxEquals(const SerializedValue& other, double epsilon = 1e-5) const;

	bool operator==(const SerializedValue& other) const;
	bool operator!=(const SerializedValue& other) const {
		return !(*this == other);
	}

   private:
	Storage m_data;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_SERIALIZED_VALUE_H
