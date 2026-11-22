#ifndef BIRD_SERIALIZATION_SCENE_SERIALIZER_H
#define BIRD_SERIALIZATION_SCENE_SERIALIZER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "../scene/Scene.h"
#include "../scene/WorldObject.h"
#include "../util/Constants.h"
#include "../util/Memory.hpp"
#include "Archive.h"
#include "PrefabInstanceComponent.h"
#include "PropertyOverride.h"
#include "UUID.h"

namespace bird {

class PrefabManager;

/**
 * Full-scene serialization and deserialization service.
 *
 * Serializes complete Scene hierarchies including:
 * - Parent-child relationships (nested Scene and WorldObject nodes)
 * - Stable 128-bit UUIDs and entity names
 * - Spatial transforms (worldPosition, localPosition, worldRotation,
 *   localRotation, scale)
 * - Camera WorldEnvironment settings
 * - All registered attached components (C++ reflected components and Dart
 *   script component references with exposed properties)
 * - Prefab instances stored compactly as `prefab_asset_id` + property delta
 *   overrides, automatically reconstructed and overlaid on scene load.
 *
 * Built atop the format-agnostic Archive interface so JSON is used during
 * development while binary archives can be plugged in without changing
 * scene or component serialization logic.
 */
class SceneSerializer {
   public:
	/** Current scene file schema version. */
	static constexpr int kSchemaVersion = 1;

	/**
	 * Serializes a Scene into any write Archive (JSON, binary, etc.).
	 * @param scene Root Scene to serialize.
	 * @param ar Write-mode Archive receiving the serialized scene.
	 * @param prefabs Optional PrefabManager for computing prefab instance diffs
	 *                (defaults to PrefabManager::get()).
	 */
	BIRD_NO_EXPORT static void serializeScene(const Scene& scene, Archive& ar,
											  PrefabManager* prefabs = nullptr);

	/**
	 * Deserializes a Scene from any read Archive.
	 * @param ar Read-mode Archive positioned at the scene root.
	 * @param prefabs Optional PrefabManager for instantiating prefab instances
	 *                (defaults to PrefabManager::get()).
	 * @return Newly constructed Scene hierarchy, or nullptr on error.
	 */
	BIRD_NO_EXPORT static _own<Scene> deserializeScene(
		Archive& ar, PrefabManager* prefabs = nullptr);

	/**
	 * Serializes a single entity subtree in raw (expanded) form, ignoring
	 * prefab instance compression. Used by Prefab::createFromEntity() and
	 * internal scene traversal.
	 */
	BIRD_NO_EXPORT static void serializeEntityRaw(const WorldObject& entity,
												  Archive& ar);

	/**
	 * Deserializes an entity node as part of Prefab instantiation, remapping
	 * local UUIDs via the archive's SerializationContext, attaching
	 * PrefabInstanceComponent metadata, and applying overrides BEFORE init().
	 */
	BIRD_NO_EXPORT static _own<WorldObject> deserializeEntityFromPrefab(
		Archive& ar, const AssetID& prefabAssetId, bool isInstanceRoot,
		const std::unordered_map<UUID, std::vector<PropertyOverride>>&
			overridesByLocalUuid);

	/**
	 * Serializes a Scene to a human-readable JSON string.
	 * @param scene Scene to serialize (must be non-null).
	 * @param indent Spaces per indent level (2 by default; < 0 for compact).
	 * @return Formatted JSON string, or empty string if scene is null.
	 */
	static std::string serializeSceneToJson(const Scene* scene, int indent = 2);

	/**
	 * Deserializes a Scene from a JSON string.
	 * @param jsonText JSON document produced by serializeSceneToJson().
	 * @param errorOut Optional output receiving parse/validation errors.
	 * @return Newly constructed Scene, or nullptr on failure.
	 */
	BIRD_NO_EXPORT static _own<Scene> deserializeSceneFromJson(
		const std::string& jsonText, std::string* errorOut = nullptr);

	/**
	 * Saves a Scene as JSON to a file on disk.
	 * @param scene Scene to save (must be non-null).
	 * @param filePath Destination path.
	 * @return True if written successfully.
	 */
	static bool saveSceneToFile(const Scene* scene, const std::string& filePath);

	/**
	 * Loads a Scene from a JSON file (via platform VFS or direct filesystem).
	 * @param filePath Relative VFS path or filesystem path.
	 * @param errorOut Optional error output on failure.
	 * @return Newly constructed Scene, or nullptr on failure.
	 */
	BIRD_NO_EXPORT static _own<Scene> loadSceneFromFile(
		const std::string& filePath, std::string* errorOut = nullptr);

	/**
	 * Recursively searches `root` (including `root` itself) for an entity
	 * matching `uuid`.
	 * @return Non-owning pointer to the matching entity, or nullptr.
	 */
	BIRD_NO_EXPORT static WorldObject* findByUUID(Scene& root, const UUID& uuid);
	BIRD_NO_EXPORT static const WorldObject* findByUUID(const Scene& root,
														const UUID& uuid);

	/**
	 * Dart-friendly lookup searching `root` for an entity by canonical UUID string.
	 * @return Non-owning pointer to the matching entity, or nullptr.
	 */
	static WorldObject* findByUUIDString(Scene* root,
										 const std::string& uuidStr);
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_SCENE_SERIALIZER_H
