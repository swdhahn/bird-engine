#include "SceneSerializer.h"

#include <fstream>
#include <sstream>

#include "../platform/VFS.h"
#include "../scene/Camera.h"
#include "BuiltinComponents.h"
#include "ComponentRegistry.h"
#include "JsonArchive.h"
#include "PrefabManager.h"

namespace bird {

namespace {

std::string objectTypeToString(OBJ_TYPES type) {
	switch (type) {
		case SCENE:
			return "Scene";
		case CAMERA:
			return "Camera";
		case WORLDOBJECT:
		default:
			return "WorldObject";
	}
}

void serializeTransform(const WorldObject& entity, Archive& ar) {
	if (ar.beginObject("transform")) {
		Vector3 worldPos = entity.getWorldPosition();
		Vector3 localPos = entity.getLocalPosition();
		Quaternion worldRot = entity.getWorldRotation();
		Quaternion localRot = entity.getLocalRotation();
		Vector3 scale = entity.getScale();
		ar.field("worldPosition", worldPos);
		ar.field("localPosition", localPos);
		ar.field("worldRotation", worldRot);
		ar.field("localRotation", localRot);
		ar.field("scale", scale);
		ar.endObject();
	}
}

void deserializeTransform(WorldObject& entity, Archive& ar) {
	if (ar.beginObject("transform")) {
		Vector3 worldPos = entity.getWorldPosition();
		Vector3 localPos = entity.getLocalPosition();
		Quaternion worldRot = entity.getWorldRotation();
		Quaternion localRot = entity.getLocalRotation();
		Vector3 scale = entity.getScale();
		if (ar.field("worldPosition", worldPos)) entity.setWorldPosition(worldPos);
		if (ar.field("localPosition", localPos)) entity.setLocalPosition(localPos);
		if (ar.field("worldRotation", worldRot)) entity.setWorldRotation(worldRot);
		if (ar.field("localRotation", localRot)) entity.setLocalRotation(localRot);
		if (ar.field("scale", scale)) entity.setScale(scale);
		ar.endObject();
	}
}

void serializeCameraData(const Camera& cam, Archive& ar) {
	if (ar.beginObject("camera")) {
		// getWorldEnvironment() is non-const on Camera; read-only access here.
		WorldEnvironment env = const_cast<Camera&>(cam).getWorldEnvironment();
		ar.field("clearColor", env.clearColor);
		ar.field("ambientColor", env.ambientColor);
		ar.field("ambientIntensity", env.ambientIntensity);
		ar.field("exposure", env.exposure);
		ar.field("debugView", env.debugView);
		ar.endObject();
	}
}

void deserializeCameraData(Camera& cam, Archive& ar) {
	if (ar.beginObject("camera")) {
		WorldEnvironment env = cam.getWorldEnvironment();
		ar.field("clearColor", env.clearColor);
		ar.field("ambientColor", env.ambientColor);
		ar.field("ambientIntensity", env.ambientIntensity);
		ar.field("exposure", env.exposure);
		ar.field("debugView", env.debugView);
		cam.setWorldEnvironment(env);
		ar.endObject();
	}
}

void serializeComponents(const WorldObject& entity, Archive& ar) {
	const auto& reg = ComponentRegistry::instance();
	auto& components = const_cast<WorldObject&>(entity).getComponents();

	std::vector<const Component*> serializable;
	serializable.reserve(components.size());
	for (const auto& compPtr : components) {
		if (!compPtr) continue;
		// PrefabInstanceComponent is scene-level metadata, not a user component.
		if (dynamic_cast<const PrefabInstanceComponent*>(compPtr.get())) {
			continue;
		}
		if (reg.findForComponent(*compPtr) != nullptr) {
			serializable.push_back(compPtr.get());
		}
	}

	size_t count = serializable.size();
	if (ar.beginArray("components", count)) {
		for (size_t i = 0; i < serializable.size(); ++i) {
			const Component* comp = serializable[i];
			const ComponentDescriptor* desc = reg.findForComponent(*comp);
			if (!desc) continue;
			if (ar.beginArrayElement(i)) {
				std::string typeName = desc->typeName;
				ar.field("type", typeName);
				if (ar.beginObject("data")) {
					reg.serializeComponent(*comp, ar);
					ar.endObject();
				}
				ar.endArrayElement();
			}
		}
		ar.endArray();
	}
}

void deserializeComponents(WorldObject& entity, Archive& ar) {
	const auto& reg = ComponentRegistry::instance();
	size_t count = 0;
	if (!ar.beginArray("components", count)) return;

	for (size_t i = 0; i < count; ++i) {
		if (!ar.beginArrayElement(i)) continue;
		std::string typeName;
		if (ar.field("type", typeName) && ar.beginObject("data")) {
			if (auto comp = reg.deserializeComponent(typeName, ar)) {
				entity.addComponent(_mv(comp));
			}
			ar.endObject();
		}
		ar.endArrayElement();
	}
	ar.endArray();
}

// ------------------------------------------------ Prefab Instance Auto-Diffing

void indexLivePrefabEntities(
	const WorldObject& entity,
	std::unordered_map<UUID, const WorldObject*>& localToLive,
	std::unordered_map<UUID, UUID>& liveUuidToLocal) {
	if (const auto* pic = findPrefabInstanceComponent(entity)) {
		localToLive[pic->getSourceLocalUuid()] = &entity;
		liveUuidToLocal[entity.getUUID()] = pic->getSourceLocalUuid();
	}
	if (entity.getType() == SCENE) {
		auto& scene = const_cast<Scene&>(static_cast<const Scene&>(entity));
		for (const auto& child : scene.getChildren()) {
			if (child) {
				indexLivePrefabEntities(*child, localToLive, liveUuidToLocal);
			}
		}
	}
}

void diffEntityAgainstBase(
	const WorldObject& baseEntity, const WorldObject& liveEntity,
	const UUID& localUuid, const UUID& rootLocalUuid,
	const std::unordered_map<UUID, UUID>& baseLiveToLocal,
	const std::unordered_map<UUID, UUID>& liveUuidToLocal,
	std::vector<PropertyOverride>& outOverrides) {
	const UUID targetId = (localUuid == rootLocalUuid) ? UUID::nil() : localUuid;

	// 1. Compare entity-level properties.
	if (liveEntity.getName() != baseEntity.getName()) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"name", SerializedValue(liveEntity.getName()), targetId));
	}
	if (!SerializedValue(liveEntity.getWorldPosition())
			 .approxEquals(SerializedValue(baseEntity.getWorldPosition()))) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"worldPosition", SerializedValue(liveEntity.getWorldPosition()),
			targetId));
	}
	if (!SerializedValue(liveEntity.getLocalPosition())
			 .approxEquals(SerializedValue(baseEntity.getLocalPosition()))) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"localPosition", SerializedValue(liveEntity.getLocalPosition()),
			targetId));
	}
	if (!SerializedValue(liveEntity.getWorldRotation())
			 .approxEquals(SerializedValue(baseEntity.getWorldRotation()))) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"worldRotation", SerializedValue(liveEntity.getWorldRotation()),
			targetId));
	}
	if (!SerializedValue(liveEntity.getLocalRotation())
			 .approxEquals(SerializedValue(baseEntity.getLocalRotation()))) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"localRotation", SerializedValue(liveEntity.getLocalRotation()),
			targetId));
	}
	if (!SerializedValue(liveEntity.getScale())
			 .approxEquals(SerializedValue(baseEntity.getScale()))) {
		outOverrides.push_back(PropertyOverride::entityProperty(
			"scale", SerializedValue(liveEntity.getScale()), targetId));
	}

	// 2. Compare registered components grouped by (typeName, index).
	const auto& reg = ComponentRegistry::instance();

	auto groupComponents = [&](const WorldObject& obj) {
		std::unordered_map<std::string, std::vector<const Component*>> groups;
		auto& comps = const_cast<WorldObject&>(obj).getComponents();
		for (const auto& c : comps) {
			if (!c) continue;
			if (dynamic_cast<const PrefabInstanceComponent*>(c.get())) continue;
			if (const ComponentDescriptor* desc = reg.findForComponent(*c)) {
				groups[desc->typeName].push_back(c.get());
			}
		}
		return groups;
	};

	auto baseGroups = groupComponents(baseEntity);
	auto liveGroups = groupComponents(liveEntity);

	auto normalizeEntityRef =
		[](const SerializedValue& val,
		   const std::unordered_map<UUID, UUID>& mapToLocal) -> SerializedValue {
		if (val.isEntityRef()) {
			const UUID id = val.asEntityRef().uuid;
			auto it = mapToLocal.find(id);
			if (it != mapToLocal.end()) {
				return SerializedValue(EntityRef(it->second));
			}
		}
		return val;
	};

	for (const auto& [typeName, liveList] : liveGroups) {
		auto baseIt = baseGroups.find(typeName);
		if (baseIt == baseGroups.end()) continue;
		const auto& baseList = baseIt->second;

		const size_t pairCount = std::min(liveList.size(), baseList.size());
		for (size_t idx = 0; idx < pairCount; ++idx) {
			auto liveFields = reg.enumerateFields(*liveList[idx]);
			for (const auto& [fieldName, liveVal] : liveFields) {
				auto baseOpt = reg.getFieldValue(*baseList[idx], fieldName);
				const SerializedValue normLive =
					normalizeEntityRef(liveVal, liveUuidToLocal);
				const SerializedValue normBase =
					baseOpt ? normalizeEntityRef(*baseOpt, baseLiveToLocal)
							: SerializedValue();

				if (!baseOpt || !normLive.approxEquals(normBase)) {
					// Preserve external UUIDs; normalize internal prefab refs to local UUID.
					outOverrides.push_back(PropertyOverride::componentField(
						typeName, fieldName, normLive, targetId,
						static_cast<uint32_t>(idx)));
				}
			}
		}
	}
}

void diffPrefabSubtreeRecursive(
	const WorldObject& baseNode,
	const std::unordered_map<UUID, const WorldObject*>& localToLive,
	const UUID& rootLocalUuid,
	const std::unordered_map<UUID, UUID>& baseLiveToLocal,
	const std::unordered_map<UUID, UUID>& liveUuidToLocal,
	std::vector<PropertyOverride>& outOverrides) {
	if (const auto* pic = findPrefabInstanceComponent(baseNode)) {
		const UUID localId = pic->getSourceLocalUuid();
		auto it = localToLive.find(localId);
		if (it != localToLive.end() && it->second != nullptr) {
			diffEntityAgainstBase(baseNode, *it->second, localId, rootLocalUuid,
								  baseLiveToLocal, liveUuidToLocal,
								  outOverrides);
		}
	}
	if (baseNode.getType() == SCENE) {
		auto& scene = const_cast<Scene&>(static_cast<const Scene&>(baseNode));
		for (const auto& child : scene.getChildren()) {
			if (child) {
				diffPrefabSubtreeRecursive(*child, localToLive, rootLocalUuid,
										   baseLiveToLocal, liveUuidToLocal,
										   outOverrides);
			}
		}
	}
}

std::vector<PropertyOverride> computePrefabOverrides(
	const WorldObject& liveRoot, const PrefabInstanceComponent& pic,
	PrefabManager* prefabs) {
	if (!prefabs) return pic.getOverrides();

	auto prefab = prefabs->getPrefab(pic.getPrefabAssetId());
	if (!prefab) return pic.getOverrides();

	// Instantiate a clean base reference copy (detached, no init() side effects)
	// and diff every entity & component field against the live instance.
	auto baseRoot = prefabs->instantiatePrefabDetached(pic.getPrefabAssetId());
	if (!baseRoot) return pic.getOverrides();

	std::unordered_map<UUID, const WorldObject*> liveLocalToPtr;
	std::unordered_map<UUID, UUID> liveUuidToLocal;
	indexLivePrefabEntities(liveRoot, liveLocalToPtr, liveUuidToLocal);

	std::unordered_map<UUID, const WorldObject*> baseLocalToPtr;
	std::unordered_map<UUID, UUID> baseLiveToLocal;
	indexLivePrefabEntities(*baseRoot, baseLocalToPtr, baseLiveToLocal);

	std::vector<PropertyOverride> computed;
	diffPrefabSubtreeRecursive(*baseRoot, liveLocalToPtr,
							   prefab->getRootLocalUUID(), baseLiveToLocal,
							   liveUuidToLocal, computed);
	return computed;
}

void serializeEntityNode(const WorldObject& entity, Archive& ar,
						 PrefabManager* prefabs, bool allowPrefabCompression) {
	// If this entity is the root of a prefab instance, store only
	// `prefab_asset_id`, instance `uuid`, and property delta `overrides`.
	if (allowPrefabCompression) {
		if (const auto* pic = findPrefabInstanceComponent(entity);
			pic && pic->isInstanceRoot()) {
			AssetID assetId = pic->getPrefabAssetId();
			UUID instanceUuid = entity.getUUID();
			std::vector<PropertyOverride> overrides =
				computePrefabOverrides(entity, *pic, prefabs);

			ar.field("prefab_asset_id", assetId);
			ar.field("uuid", instanceUuid);

			size_t ovCount = overrides.size();
			if (ar.beginArray("overrides", ovCount)) {
				for (size_t i = 0; i < overrides.size(); ++i) {
					if (ar.beginArrayElement(i)) {
						overrides[i].serialize(ar);
						ar.endArrayElement();
					}
				}
				ar.endArray();
			}
			return;
		}
	}

	UUID uuid = entity.getUUID();
	std::string name = entity.getName();
	std::string objType = objectTypeToString(entity.getType());

	ar.field("uuid", uuid);
	ar.field("name", name);
	ar.field("objectType", objType);

	serializeTransform(entity, ar);

	if (entity.getType() == CAMERA) {
		serializeCameraData(static_cast<const Camera&>(entity), ar);
	}

	serializeComponents(entity, ar);

	if (entity.getType() == SCENE) {
		auto& scene = const_cast<Scene&>(static_cast<const Scene&>(entity));
		const auto& children = scene.getChildren();
		size_t childCount = children.size();
		if (ar.beginArray("children", childCount)) {
			for (size_t i = 0; i < children.size(); ++i) {
				if (!children[i]) continue;
				if (ar.beginArrayElement(i)) {
					serializeEntityNode(*children[i], ar, prefabs,
										allowPrefabCompression);
					ar.endArrayElement();
				}
			}
			ar.endArray();
		}
	}
}

_own<WorldObject> createObjectByType(const std::string& objType) {
	if (objType == "Scene") {
		return _own_mk<Scene>();
	}
	if (objType == "Camera") {
		return _own_mk<Camera>(1280, 720);
	}
	return _own_mk<WorldObject>();
}

_own<WorldObject> deserializeEntityNode(Archive& ar, PrefabManager* prefabs) {
	// Check if this entry is a prefab instance reference.
	if (ar.hasField("prefab_asset_id")) {
		AssetID assetId;
		ar.field("prefab_asset_id", assetId);

		UUID instanceUuid = UUID::nil();
		ar.field("uuid", instanceUuid);

		std::vector<PropertyOverride> overrides;
		size_t ovCount = 0;
		if (ar.beginArray("overrides", ovCount)) {
			overrides.reserve(ovCount);
			for (size_t i = 0; i < ovCount; ++i) {
				if (ar.beginArrayElement(i)) {
					PropertyOverride ov;
					ov.serialize(ar);
					overrides.push_back(std::move(ov));
					ar.endArrayElement();
				}
			}
			ar.endArray();
		}

		PrefabManager* mgr = prefabs ? prefabs : PrefabManager::get();
		std::optional<UUID> forcedRoot =
			instanceUuid.isNil() ? std::nullopt
								 : std::optional<UUID>(instanceUuid);
		return mgr->instantiatePrefabDetached(assetId, overrides, forcedRoot);
	}

	std::string objType = "WorldObject";
	ar.field("objectType", objType);

	_own<WorldObject> entity = createObjectByType(objType);
	if (!entity) return nullptr;

	UUID uuid = UUID::nil();
	if (ar.field("uuid", uuid) && !uuid.isNil()) {
		entity->setUUID(uuid);
	}

	std::string name;
	if (ar.field("name", name)) {
		entity->setName(name);
	}

	deserializeTransform(*entity, ar);

	if (entity->getType() == CAMERA) {
		deserializeCameraData(static_cast<Camera&>(*entity), ar);
	}

	deserializeComponents(*entity, ar);

	if (entity->getType() == SCENE) {
		auto* scene = static_cast<Scene*>(entity.get());
		size_t childCount = 0;
		if (ar.beginArray("children", childCount)) {
			for (size_t i = 0; i < childCount; ++i) {
				if (ar.beginArrayElement(i)) {
					if (auto child = deserializeEntityNode(ar, prefabs)) {
						scene->addChild(_mv(child));
					}
					ar.endArrayElement();
				}
			}
			ar.endArray();
		}
	}

	return entity;
}

}  // namespace

void SceneSerializer::serializeEntityRaw(const WorldObject& entity,
										 Archive& ar) {
	registerBuiltinComponents();
	serializeEntityNode(entity, ar, nullptr, /*allowPrefabCompression=*/false);
}

_own<WorldObject> SceneSerializer::deserializeEntityFromPrefab(
	Archive& ar, const AssetID& prefabAssetId, bool isInstanceRoot,
	const std::unordered_map<UUID, std::vector<PropertyOverride>>&
		overridesByLocalUuid) {
	registerBuiltinComponents();

	std::string objType = "WorldObject";
	ar.field("objectType", objType);

	_own<WorldObject> entity = createObjectByType(objType);
	if (!entity) return nullptr;

	UUID localUuid = UUID::nil();
	ar.field("uuid", localUuid);

	UUID runtimeUuid = localUuid;
	if (auto* ctx = ar.context()) {
		runtimeUuid = ctx->remapUUID(localUuid);
	}
	if (runtimeUuid.isNil()) {
		runtimeUuid = UUID::generate();
	}
	entity->setUUID(runtimeUuid);

	std::string name;
	if (ar.field("name", name)) {
		entity->setName(name);
	}

	deserializeTransform(*entity, ar);

	if (entity->getType() == CAMERA) {
		deserializeCameraData(static_cast<Camera&>(*entity), ar);
	}

	deserializeComponents(*entity, ar);

	// Attach PrefabInstanceComponent metadata so SceneSerializer recognizes
	// this entity (and its children) when saving scenes.
	entity->addComponent(_own_mk<PrefabInstanceComponent>(
		prefabAssetId, localUuid, isInstanceRoot));

	// Apply any PropertyOverrides targeting this entity's local UUID BEFORE
	// children are attached or init() is triggered.
	auto ovIt = overridesByLocalUuid.find(localUuid);
	if (ovIt != overridesByLocalUuid.end()) {
		for (const auto& ov : ovIt->second) {
			applyOverrideToEntity(*entity, ov, ar.context());
		}
	}

	// Recursively deserialize child entities inside Scene nodes.
	if (entity->getType() == SCENE) {
		auto* scene = static_cast<Scene*>(entity.get());
		size_t childCount = 0;
		if (ar.beginArray("children", childCount)) {
			for (size_t i = 0; i < childCount; ++i) {
				if (ar.beginArrayElement(i)) {
					if (auto child = deserializeEntityFromPrefab(
							ar, prefabAssetId, /*isInstanceRoot=*/false,
							overridesByLocalUuid)) {
						scene->addChild(_mv(child));
					}
					ar.endArrayElement();
				}
			}
			ar.endArray();
		}
	}

	return entity;
}

void SceneSerializer::serializeScene(const Scene& scene, Archive& ar,
									 PrefabManager* prefabs) {
	registerBuiltinComponents();
	PrefabManager* mgr = prefabs ? prefabs : PrefabManager::get();

	std::string format = "bird_scene";
	int32_t version = kSchemaVersion;
	ar.field("format", format);
	ar.field("schema_version", version);

	if (ar.beginObject("scene")) {
		serializeEntityNode(scene, ar, mgr, /*allowPrefabCompression=*/true);
		ar.endObject();
	}
}

_own<Scene> SceneSerializer::deserializeScene(Archive& ar,
											  PrefabManager* prefabs) {
	registerBuiltinComponents();
	PrefabManager* mgr = prefabs ? prefabs : PrefabManager::get();

	if (!ar.beginObject("scene")) return nullptr;

	_own<WorldObject> rootObj = deserializeEntityNode(ar, mgr);
	ar.endObject();

	if (!rootObj || rootObj->getType() != SCENE) return nullptr;
	return _own<Scene>(static_cast<Scene*>(rootObj.release()));
}

std::string SceneSerializer::serializeSceneToJson(const Scene* scene,
												  int indent) {
	if (!scene) return {};
	JsonWriteArchive ar;
	serializeScene(*scene, ar, PrefabManager::get());
	return ar.toString(indent);
}

_own<Scene> SceneSerializer::deserializeSceneFromJson(
	const std::string& jsonText, std::string* errorOut) {
	auto parsed = JsonValue::parse(jsonText, errorOut);
	if (!parsed) return nullptr;
	JsonReadArchive ar(*parsed);
	auto scene = deserializeScene(ar, PrefabManager::get());
	if (!scene && errorOut) {
		*errorOut = "Scene JSON is missing required 'scene' root object";
	}
	return scene;
}

bool SceneSerializer::saveSceneToFile(const Scene* scene,
									  const std::string& filePath) {
	if (!scene) return false;
	const std::string json = serializeSceneToJson(scene, 2);
	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) return false;
	out.write(json.data(), static_cast<std::streamsize>(json.size()));
	return out.good();
}

_own<Scene> SceneSerializer::loadSceneFromFile(const std::string& filePath,
											   std::string* errorOut) {
	std::string text;
	if (auto bytes = platform::VFS::get().readFile(filePath)) {
		text.assign(reinterpret_cast<const char*>(bytes->data()),
					bytes->size());
	} else {
		std::ifstream in(filePath, std::ios::binary);
		if (!in.is_open()) {
			if (errorOut) *errorOut = "Failed to open scene file: " + filePath;
			return nullptr;
		}
		std::ostringstream oss;
		oss << in.rdbuf();
		text = oss.str();
	}
	return deserializeSceneFromJson(text, errorOut);
}

WorldObject* SceneSerializer::findByUUID(Scene& root, const UUID& uuid) {
	if (uuid.isNil()) return nullptr;
	if (root.getUUID() == uuid) return &root;
	for (auto& child : root.getChildren()) {
		if (!child) continue;
		if (child->getUUID() == uuid) return child.get();
		if (child->getType() == SCENE) {
			if (auto* found =
					findByUUID(static_cast<Scene&>(*child), uuid)) {
				return found;
			}
		}
	}
	return nullptr;
}

const WorldObject* SceneSerializer::findByUUID(const Scene& root,
											   const UUID& uuid) {
	return findByUUID(const_cast<Scene&>(root), uuid);
}

WorldObject* SceneSerializer::findByUUIDString(Scene* root,
											   const std::string& uuidStr) {
	if (!root) return nullptr;
	auto parsed = UUID::tryParse(uuidStr);
	if (!parsed) return nullptr;
	return findByUUID(*root, *parsed);
}

}  // namespace bird
