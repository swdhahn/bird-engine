#include "PrefabManager.h"

#include <fstream>
#include <sstream>

#include "../platform/VFS.h"
#include "BuiltinComponents.h"
#include "JsonArchive.h"
#include "SceneSerializer.h"

namespace bird {

PrefabManager* PrefabManager::get() {
	static PrefabManager s_manager;
	return &s_manager;
}

void PrefabManager::registerPrefab(const AssetID& assetId,
								   _ref<Prefab> prefab) {
	if (!prefab) return;
	prefab->setAssetId(assetId);
	m_prefabs[assetId] = std::move(prefab);
}

bool PrefabManager::registerPrefabFromEntity(const AssetID& assetId,
											 WorldObject* rootEntity) {
	if (!rootEntity || assetId.empty()) return false;
	registerBuiltinComponents();
	auto prefab = Prefab::createFromEntity(assetId, *rootEntity);
	if (!prefab) return false;
	registerPrefab(assetId, std::move(prefab));
	return true;
}

bool PrefabManager::loadPrefabFromFile(const AssetID& assetId,
									   const std::string& filePath) {
	std::string text;
	if (auto bytes = platform::VFS::get().readFile(filePath)) {
		text.assign(reinterpret_cast<const char*>(bytes->data()),
					bytes->size());
	} else {
		std::ifstream in(filePath, std::ios::binary);
		if (!in.is_open()) return false;
		std::ostringstream oss;
		oss << in.rdbuf();
		text = oss.str();
	}
	return loadPrefabFromJson(assetId, text);
}

bool PrefabManager::loadPrefabFromJson(const AssetID& assetId,
									   const std::string& jsonText) {
	auto prefab = Prefab::fromJsonString(jsonText, assetId);
	if (!prefab) return false;
	registerPrefab(assetId, std::move(prefab));
	return true;
}

bool PrefabManager::savePrefabToFile(const AssetID& assetId,
									 const std::string& filePath) const {
	auto prefab = getPrefab(assetId);
	if (!prefab) return false;
	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) return false;
	const std::string text = prefab->toJsonString(2);
	out.write(text.data(), static_cast<std::streamsize>(text.size()));
	return out.good();
}

bool PrefabManager::hasPrefab(const AssetID& assetId) const {
	return m_prefabs.find(assetId) != m_prefabs.end();
}

_ref<Prefab> PrefabManager::getPrefab(const AssetID& assetId) const {
	auto it = m_prefabs.find(assetId);
	return it != m_prefabs.end() ? it->second : nullptr;
}

void PrefabManager::unregisterPrefab(const AssetID& assetId) {
	m_prefabs.erase(assetId);
}

void PrefabManager::clear() {
	m_prefabs.clear();
}

_ref<Prefab> PrefabManager::resolvePrefab(const AssetID& assetId) {
	if (auto cached = getPrefab(assetId)) return cached;
	if (loadPrefabFromFile(assetId, assetId)) {
		return getPrefab(assetId);
	}
	return nullptr;
}

_own<WorldObject> PrefabManager::instantiatePrefabDetached(
	const AssetID& assetId, const std::vector<PropertyOverride>& overrides,
	std::optional<UUID> forcedRootUuid) {
	registerBuiltinComponents();

	auto prefab = resolvePrefab(assetId);
	if (!prefab) return nullptr;

	// 1. Build local-UUID -> freshly spawned runtime-UUID remap table for
	//    every entity in the prefab subtree before deserializing any entity.
	const UUID rootLocalUuid = prefab->getRootLocalUUID();
	const std::vector<UUID> localUuids = prefab->collectLocalUUIDs();
	std::unordered_map<UUID, UUID> uuidRemap;
	uuidRemap.reserve(localUuids.size());

	for (const UUID& localId : localUuids) {
		if (localId == rootLocalUuid && forcedRootUuid.has_value() &&
			!forcedRootUuid->isNil()) {
			uuidRemap[localId] = *forcedRootUuid;
		} else {
			uuidRemap[localId] = UUID::generate();
		}
	}

	// 2. Group overrides by their target local UUID (nil resolves to rootLocalUuid).
	std::unordered_map<UUID, std::vector<PropertyOverride>> overridesByLocalUuid;
	for (const auto& ov : overrides) {
		const UUID target =
			ov.targetLocalUuid.isNil() ? rootLocalUuid : ov.targetLocalUuid;
		overridesByLocalUuid[target].push_back(ov);
	}

	// 3. Deserialize the entity subtree with uuidRemap active, applying each
	//    entity's overrides BEFORE it is attached / initialized.
	SerializationContext ctx;
	ctx.uuidRemap = &uuidRemap;
	ctx.prefabManager = this;

	JsonReadArchive ar(prefab->getRootEntityData());
	ar.setContext(&ctx);

	_own<WorldObject> root = SceneSerializer::deserializeEntityFromPrefab(
		ar, assetId, /*isInstanceRoot=*/true, overridesByLocalUuid);
	if (!root) return nullptr;

	// Ensure the root carries the complete override list on its PrefabInstanceComponent.
	if (auto* pic = findPrefabInstanceComponent(*root)) {
		pic->setOverrides(overrides);
	}
	return root;
}

WorldObject* PrefabManager::instantiatePrefabWithOverrides(
	const AssetID& assetId, Scene* parentScene,
	const std::vector<PropertyOverride>& overrides,
	std::optional<UUID> forcedRootUuid) {
	if (!parentScene) return nullptr;
	_own<WorldObject> root =
		instantiatePrefabDetached(assetId, overrides, forcedRootUuid);
	if (!root) return nullptr;
	WorldObject* raw = root.get();
	// Scene::addChild() transfers ownership and triggers root->init(parentScene),
	// initializing all components and Dart scripts with remapped IDs & overrides!
	parentScene->addChild(_mv(root));
	return raw;
}

WorldObject* PrefabManager::instantiatePrefab(const AssetID& assetId,
											  Scene* parentScene) {
	return instantiatePrefabWithOverrides(assetId, parentScene, {});
}

WorldObject* InstantiatePrefab(const AssetID& assetId, Scene* parentScene) {
	return PrefabManager::get()->instantiatePrefab(assetId, parentScene);
}

}  // namespace bird
