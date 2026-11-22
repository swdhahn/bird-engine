#ifndef BIRD_SERIALIZATION_PREFAB_H
#define BIRD_SERIALIZATION_PREFAB_H

#include <optional>
#include <string>
#include <vector>

#include "../util/Memory.hpp"
#include "Json.h"
#include "PrefabInstanceComponent.h"
#include "UUID.h"

namespace bird {

class WorldObject;

/**
 * Reusable archetype asset storing a serialized entity subtree with stable
 * local UUIDs for every entity inside the prefab.
 *
 * A Prefab captures an entity (which may be a WorldObject, Camera, or nested
 * Scene containing child entities), all of its transforms, and all registered
 * attached components (including Dart script references and exposed properties).
 *
 * When instantiated via PrefabManager::instantiatePrefab(), every local UUID
 * inside the prefab is remapped to a freshly generated runtime UUID, and all
 * internal EntityRef component fields are rewritten to point to the newly
 * spawned entities.
 */
class Prefab {
   public:
	/** Current prefab JSON schema version. */
	static constexpr int kSchemaVersion = 1;

	Prefab() = default;

	/**
	 * Constructs a Prefab from an AssetID and a serialized root entity JsonValue.
	 * @param assetId Unique AssetID identifying this prefab asset.
	 * @param rootEntityJson Serialized entity tree produced by SceneSerializer.
	 */
	Prefab(AssetID assetId, JsonValue rootEntityJson);

	/**
	 * Captures a live WorldObject (or Scene subtree) as a reusable Prefab asset.
	 * The entity's current UUIDs become the prefab's stable local UUIDs.
	 *
	 * @param assetId AssetID to assign to the newly created prefab.
	 * @param rootEntity Live root entity to snapshot.
	 * @return Shared pointer to the newly constructed Prefab.
	 */
	static _ref<Prefab> createFromEntity(const AssetID& assetId,
										 const WorldObject& rootEntity);

	/**
	 * Deserializes a Prefab from a parsed JsonValue document.
	 * @param doc Root JSON object (must contain `"root"`).
	 * @param fallbackAssetId AssetID to use if the document omits `"prefab_asset_id"`.
	 * @return Shared pointer to the Prefab, or nullptr if malformed.
	 */
	static _ref<Prefab> fromJson(const JsonValue& doc,
								 const AssetID& fallbackAssetId = "");

	/**
	 * Parses a Prefab from a JSON text string.
	 * @param jsonText JSON document text.
	 * @param fallbackAssetId AssetID to use if omitted from the document.
	 * @param errorOut Optional error message output on parse/validation failure.
	 * @return Shared pointer to the Prefab, or nullptr on failure.
	 */
	static _ref<Prefab> fromJsonString(const std::string& jsonText,
									   const AssetID& fallbackAssetId = "",
									   std::string* errorOut = nullptr);

	/** Serializes this Prefab to a JsonValue envelope. */
	JsonValue toJson() const;

	/** Serializes this Prefab to formatted (or compact) JSON text. */
	std::string toJsonString(int indent = 2) const;

	/** Returns the AssetID of this prefab. */
	const AssetID& getAssetId() const noexcept { return m_assetId; }

	/** Updates the AssetID of this prefab. */
	void setAssetId(AssetID assetId) { m_assetId = std::move(assetId); }

	/** Returns the serialized root entity JSON subtree. */
	const JsonValue& getRootEntityData() const noexcept { return m_rootEntity; }

	/**
	 * Returns the stable local UUID of the prefab's root entity, or UUID::nil()
	 * if not present.
	 */
	UUID getRootLocalUUID() const;

	/**
	 * Recursively collects every stable local UUID defined inside this prefab
	 * subtree (root and all descendants).
	 */
	std::vector<UUID> collectLocalUUIDs() const;

   private:
	AssetID m_assetId;
	int m_schemaVersion = kSchemaVersion;
	JsonValue m_rootEntity;
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_PREFAB_H
