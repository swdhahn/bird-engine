#ifndef BIRD_SERIALIZATION_JSON_ARCHIVE_H
#define BIRD_SERIALIZATION_JSON_ARCHIVE_H

#include <string>
#include <vector>

#include "Archive.h"
#include "Json.h"

namespace bird {

/**
 * Converts a SerializedValue into its canonical JsonValue representation.
 *
 * Tagged wrappers are used for types that JSON cannot distinguish natively
 * (Vec2/3/4, Quat, UUID, EntityRef) so dynamic property bags and prefab
 * overrides round-trip with exact type fidelity.
 */
JsonValue serializedValueToJson(const SerializedValue& value);

/**
 * Reconstructs a SerializedValue from a JsonValue produced by
 * serializedValueToJson(), optionally remapping EntityRef values via ctx.
 */
SerializedValue jsonToSerializedValue(const JsonValue& json,
									  const SerializationContext* ctx = nullptr);

/**
 * Concrete Archive writing C++ data into a human-readable JsonValue tree.
 *
 * Call root() or toString() after writing to obtain the resulting JSON.
 */
class JsonWriteArchive final : public Archive {
   public:
	/** Constructs a write archive rooted at an empty JSON object `{}`. */
	JsonWriteArchive();

	ArchiveMode mode() const noexcept override { return ArchiveMode::Write; }
	bool hasField(const char* /*name*/) const override { return true; }

	bool beginObject(const char* name) override;
	void endObject() override;
	bool beginArray(const char* name, size_t& count) override;
	void endArray() override;
	bool beginArrayElement(size_t index) override;
	void endArrayElement() override;

	bool field(const char* name, bool& value) override;
	bool field(const char* name, int32_t& value) override;
	bool field(const char* name, uint32_t& value) override;
	bool field(const char* name, int64_t& value) override;
	bool field(const char* name, uint64_t& value) override;
	bool field(const char* name, float& value) override;
	bool field(const char* name, double& value) override;
	bool field(const char* name, std::string& value) override;
	bool field(const char* name, UUID& value) override;
	bool field(const char* name, Vector2& value) override;
	bool field(const char* name, Vector3& value) override;
	bool field(const char* name, Vector4& value) override;
	bool field(const char* name, Quaternion& value) override;
	bool field(const char* name, Matrix4& value) override;
	bool field(const char* name, EntityRef& value) override;
	bool field(const char* name, SerializedValue& value) override;

	/** Returns the constructed root JsonValue object. */
	const JsonValue& root() const noexcept { return m_root; }
	JsonValue& root() noexcept { return m_root; }

	/**
	 * Formats the root JSON object as a string.
	 * @param indent Spaces per indent level (2 by default; < 0 for compact).
	 */
	std::string toString(int indent = 2) const { return m_root.dump(indent); }

   private:
	JsonValue& current();
	void writeKeyValue(const char* name, JsonValue val);

	JsonValue m_root;
	std::vector<JsonValue*> m_stack;
};

/**
 * Concrete Archive reading C++ data from a parsed JsonValue tree.
 *
 * Missing fields leave the target C++ variable unchanged and return false,
 * allowing forward-compatible schema evolution with sensible defaults.
 */
class JsonReadArchive final : public Archive {
   public:
	/**
	 * Constructs a read archive viewing an existing JsonValue root.
	 * The caller must keep `root` alive for the lifetime of this archive.
	 */
	explicit JsonReadArchive(const JsonValue& root);

	ArchiveMode mode() const noexcept override { return ArchiveMode::Read; }
	bool hasField(const char* name) const override;

	bool beginObject(const char* name) override;
	void endObject() override;
	bool beginArray(const char* name, size_t& count) override;
	void endArray() override;
	bool beginArrayElement(size_t index) override;
	void endArrayElement() override;

	bool field(const char* name, bool& value) override;
	bool field(const char* name, int32_t& value) override;
	bool field(const char* name, uint32_t& value) override;
	bool field(const char* name, int64_t& value) override;
	bool field(const char* name, uint64_t& value) override;
	bool field(const char* name, float& value) override;
	bool field(const char* name, double& value) override;
	bool field(const char* name, std::string& value) override;
	bool field(const char* name, UUID& value) override;
	bool field(const char* name, Vector2& value) override;
	bool field(const char* name, Vector3& value) override;
	bool field(const char* name, Vector4& value) override;
	bool field(const char* name, Quaternion& value) override;
	bool field(const char* name, Matrix4& value) override;
	bool field(const char* name, EntityRef& value) override;
	bool field(const char* name, SerializedValue& value) override;

   private:
	const JsonValue* current() const;
	const JsonValue* findField(const char* name) const;

	std::vector<const JsonValue*> m_stack;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_JSON_ARCHIVE_H
