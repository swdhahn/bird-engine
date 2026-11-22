#ifndef BIRD_SERIALIZATION_JSON_H
#define BIRD_SERIALIZATION_JSON_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bird {

/**
 * Discriminator for the active JSON value type inside JsonValue.
 */
enum class JsonType : uint8_t {
	Null = 0,
	Bool,
	Int,
	Double,
	String,
	Array,
	Object
};

/**
 * Lightweight, dependency-free RFC 8259 JSON DOM node.
 *
 * Objects preserve insertion order so serialized scenes and prefabs produce
 * deterministic, diff-friendly output in version control. Designed to work
 * across all Bird Engine targets (Desktop, Headless, Android, Web/Emscripten)
 * without third-party JSON dependencies.
 */
class JsonValue {
   public:
	using Array = std::vector<JsonValue>;
	using Object = std::vector<std::pair<std::string, JsonValue>>;

	/** Constructs a JSON null value. */
	JsonValue() : m_data(std::monostate{}) {}
	JsonValue(std::nullptr_t) : m_data(std::monostate{}) {}
	JsonValue(bool v) : m_data(v) {}
	JsonValue(int32_t v) : m_data(static_cast<int64_t>(v)) {}
	JsonValue(uint32_t v) : m_data(static_cast<int64_t>(v)) {}
	JsonValue(int64_t v) : m_data(v) {}
	JsonValue(float v) : m_data(static_cast<double>(v)) {}
	JsonValue(double v) : m_data(v) {}
	JsonValue(const char* v) : m_data(std::string(v ? v : "")) {}
	JsonValue(std::string v) : m_data(std::move(v)) {}
	JsonValue(Array v) : m_data(std::move(v)) {}
	JsonValue(Object v) : m_data(std::move(v)) {}

	/** Factory returning an empty JSON array `[]`. */
	static JsonValue array() { return JsonValue(Array{}); }

	/** Factory returning an empty JSON object `{}`. */
	static JsonValue object() { return JsonValue(Object{}); }

	/** Returns the active JsonType of this node. */
	JsonType type() const noexcept {
		return static_cast<JsonType>(m_data.index());
	}

	bool isNull() const noexcept { return type() == JsonType::Null; }
	bool isBool() const noexcept { return type() == JsonType::Bool; }
	bool isInt() const noexcept { return type() == JsonType::Int; }
	bool isDouble() const noexcept { return type() == JsonType::Double; }
	bool isNumber() const noexcept { return isInt() || isDouble(); }
	bool isString() const noexcept { return type() == JsonType::String; }
	bool isArray() const noexcept { return type() == JsonType::Array; }
	bool isObject() const noexcept { return type() == JsonType::Object; }

	bool asBool(bool fallback = false) const;
	int64_t asInt(int64_t fallback = 0) const;
	double asDouble(double fallback = 0.0) const;
	const std::string& asString() const;
	const Array& asArray() const;
	Array& asArray();
	const Object& asObject() const;
	Object& asObject();

	/** Returns the number of elements in an array or keys in an object (0 otherwise). */
	size_t size() const noexcept;

	/** Appends a value to a JSON array. Throws std::logic_error if not an array. */
	void pushBack(JsonValue value);

	/**
	 * Sets or inserts a key/value pair on a JSON object, preserving insertion
	 * order when updating an existing key. Throws std::logic_error if not an object.
	 */
	void set(const std::string& key, JsonValue value);

	/** Returns true if this is a JSON object containing the specified key. */
	bool contains(const std::string& key) const;

	/**
	 * Looks up a key on a JSON object.
	 * @return Pointer to the member value, or nullptr if missing or not an object.
	 */
	const JsonValue* find(const std::string& key) const;
	JsonValue* find(const std::string& key);

	/**
	 * Serializes this value to a JSON string.
	 * @param indent Number of spaces per indentation level (>= 0 for pretty-printed
	 *               output, or < 0 for compact single-line JSON).
	 * @return RFC 8259 compliant JSON text.
	 */
	std::string dump(int indent = 2) const;

	/**
	 * Parses a JSON text string into a JsonValue tree.
	 * @param text Input JSON string.
	 * @param errorOut Optional output receiving a human-readable error message
	 *                 with line and column numbers on parse failure.
	 * @return Parsed JsonValue, or std::nullopt on syntax error.
	 */
	static std::optional<JsonValue> parse(const std::string& text,
										  std::string* errorOut = nullptr);

	bool operator==(const JsonValue& other) const;
	bool operator!=(const JsonValue& other) const { return !(*this == other); }

   private:
	using Storage = std::variant<std::monostate, bool, int64_t, double,
								 std::string, Array, Object>;
	Storage m_data;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_JSON_H
