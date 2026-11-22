#ifndef BIRD_SERIALIZATION_ARCHIVE_H
#define BIRD_SERIALIZATION_ARCHIVE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "../util/Math.h"
#include "SerializedValue.h"
#include "UUID.h"

namespace bird {

class PrefabManager;
class WorldObject;

/**
 * Direction of an Archive stream (serializing out vs. deserializing in).
 */
enum class ArchiveMode : uint8_t {
	Write = 0,
	Read = 1
};

/**
 * Contextual state threaded through an Archive during scene/prefab operations.
 *
 * Primarily carries the UUID remapping table used during prefab instantiation
 * so EntityRef fields pointing to local prefab entities are transparently
 * rewritten to the newly spawned runtime UUIDs.
 */
struct SerializationContext {
	/**
	 * Optional mapping from source (local prefab) UUID -> runtime spawned UUID.
	 * When non-null during deserialization, EntityRef fields whose UUID appears
	 * in this map are automatically replaced with the mapped runtime UUID.
	 */
	const std::unordered_map<UUID, UUID>* uuidRemap = nullptr;

	/** Optional PrefabManager used to resolve prefab references during scene load/save. */
	PrefabManager* prefabManager = nullptr;

	/**
	 * Remaps an EntityRef using uuidRemap if present.
	 * External references not in uuidRemap are returned unchanged.
	 */
	EntityRef remapEntityRef(EntityRef ref) const {
		if (uuidRemap && !ref.isNull()) {
			auto it = uuidRemap->find(ref.uuid);
			if (it != uuidRemap->end()) {
				return EntityRef(it->second);
			}
		}
		return ref;
	}

	/**
	 * Remaps a UUID using uuidRemap if present.
	 */
	UUID remapUUID(UUID id) const {
		if (uuidRemap && !id.isNil()) {
			auto it = uuidRemap->find(id);
			if (it != uuidRemap->end()) {
				return it->second;
			}
		}
		return id;
	}
};

/**
 * Format-agnostic serialization archive interface.
 *
 * Provides a uniform bidirectional API (single `serialize(Archive&)` flow or
 * separate read/write paths) across text and binary backends. Concrete backends
 * such as JsonWriteArchive / JsonReadArchive (human-readable development
 * format) or a future BinaryWriteArchive / BinaryReadArchive implement the
 * virtual primitives below.
 *
 * Usage example:
 * @code
 * void serialize(Archive& ar) {
 *     ar.field("health", health);
 *     ar.field("position", position);
 *     ar.field("target", targetEntityRef);
 * }
 * @endcode
 */
class Archive {
   public:
	virtual ~Archive() = default;

	/** Returns whether this archive is writing or reading. */
	virtual ArchiveMode mode() const noexcept = 0;

	/** True when serializing C++ objects into the archive. */
	bool isWriting() const noexcept { return mode() == ArchiveMode::Write; }

	/** True when deserializing from the archive into C++ objects. */
	bool isReading() const noexcept { return mode() == ArchiveMode::Read; }

	/** Attaches an optional SerializationContext (not owned by the archive). */
	void setContext(SerializationContext* ctx) noexcept { m_context = ctx; }

	/** Returns the attached SerializationContext, or nullptr. */
	SerializationContext* context() const noexcept { return m_context; }

	/**
	 * Returns true if the current object scope contains a field with the given name.
	 * Always returns true in Write mode.
	 */
	virtual bool hasField(const char* name) const = 0;

	/**
	 * Opens a nested named object scope.
	 * In Read mode, returns false if the field does not exist or is not an object
	 * (in which case endObject() must NOT be called).
	 * In Write mode, always creates the object and returns true.
	 */
	virtual bool beginObject(const char* name) = 0;

	/** Closes the current nested object scope opened by beginObject(). */
	virtual void endObject() = 0;

	/**
	 * Opens a named array scope.
	 * In Write mode, uses `count` as a size hint (useful for binary formats) and returns true.
	 * In Read mode, populates `count` with the number of elements and returns true if present.
	 * If this returns false, endArray() must NOT be called.
	 */
	virtual bool beginArray(const char* name, size_t& count) = 0;

	/** Closes the current array scope opened by beginArray(). */
	virtual void endArray() = 0;

	/**
	 * Opens the i-th element inside an array scope as an object scope.
	 * Must be paired with endArrayElement() when returning true.
	 */
	virtual bool beginArrayElement(size_t index) = 0;

	/** Closes the current array element scope. */
	virtual void endArrayElement() = 0;

	/** Primitive & engine type fields. Returns true when written or read. */
	virtual bool field(const char* name, bool& value) = 0;
	virtual bool field(const char* name, int32_t& value) = 0;
	virtual bool field(const char* name, uint32_t& value) = 0;
	virtual bool field(const char* name, int64_t& value) = 0;
	virtual bool field(const char* name, uint64_t& value) = 0;
	virtual bool field(const char* name, float& value) = 0;
	virtual bool field(const char* name, double& value) = 0;
	virtual bool field(const char* name, std::string& value) = 0;
	virtual bool field(const char* name, UUID& value) = 0;
	virtual bool field(const char* name, Vector2& value) = 0;
	virtual bool field(const char* name, Vector3& value) = 0;
	virtual bool field(const char* name, Vector4& value) = 0;
	virtual bool field(const char* name, Quaternion& value) = 0;
	virtual bool field(const char* name, Matrix4& value) = 0;
	virtual bool field(const char* name, EntityRef& value) = 0;
	virtual bool field(const char* name, SerializedValue& value) = 0;

	/**
	 * Convenience overload for enum / enum class types, serializing via their
	 * underlying integer representation.
	 */
	template <typename EnumT,
			  typename = std::enable_if_t<std::is_enum_v<EnumT>>>
	bool fieldEnum(const char* name, EnumT& value) {
		int32_t raw = static_cast<int32_t>(value);
		if (!field(name, raw)) return false;
		if (isReading()) {
			value = static_cast<EnumT>(raw);
		}
		return true;
	}

   private:
	SerializationContext* m_context = nullptr;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_ARCHIVE_H
