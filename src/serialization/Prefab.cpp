#include "Prefab.h"

#include "JsonArchive.h"
#include "SceneSerializer.h"

namespace bird {

namespace {

void collectLocalUUIDsRecursive(const JsonValue& node, std::vector<UUID>& out) {
	if (!node.isObject()) return;
	if (const JsonValue* uuidVal = node.find("uuid");
		uuidVal && uuidVal->isString()) {
		if (auto parsed = UUID::tryParse(uuidVal->asString())) {
			out.push_back(*parsed);
		}
	}
	if (const JsonValue* children = node.find("children");
		children && children->isArray()) {
		for (const auto& child : children->asArray()) {
			collectLocalUUIDsRecursive(child, out);
		}
	}
}

}  // namespace

Prefab::Prefab(AssetID assetId, JsonValue rootEntityJson)
	: m_assetId(std::move(assetId)),
	  m_schemaVersion(kSchemaVersion),
	  m_rootEntity(std::move(rootEntityJson)) {}

_ref<Prefab> Prefab::createFromEntity(const AssetID& assetId,
									  const WorldObject& rootEntity) {
	JsonWriteArchive ar;
	SceneSerializer::serializeEntityRaw(rootEntity, ar);
	return _ref_mk<Prefab>(assetId, std::move(ar.root()));
}

_ref<Prefab> Prefab::fromJson(const JsonValue& doc,
							  const AssetID& fallbackAssetId) {
	if (!doc.isObject()) return nullptr;
	const JsonValue* root = doc.find("root");
	if (!root || !root->isObject()) return nullptr;

	AssetID id = fallbackAssetId;
	if (const JsonValue* idVal = doc.find("prefab_asset_id");
		idVal && idVal->isString() && !idVal->asString().empty()) {
		id = idVal->asString();
	}

	auto prefab = _ref_mk<Prefab>(std::move(id), *root);
	if (const JsonValue* ver = doc.find("schema_version");
		ver && ver->isInt()) {
		prefab->m_schemaVersion = static_cast<int>(ver->asInt());
	}
	return prefab;
}

_ref<Prefab> Prefab::fromJsonString(const std::string& jsonText,
									const AssetID& fallbackAssetId,
									std::string* errorOut) {
	auto parsed = JsonValue::parse(jsonText, errorOut);
	if (!parsed) return nullptr;
	auto prefab = fromJson(*parsed, fallbackAssetId);
	if (!prefab && errorOut) {
		*errorOut = "Prefab JSON is missing required 'root' entity object";
	}
	return prefab;
}

JsonValue Prefab::toJson() const {
	JsonValue doc = JsonValue::object();
	doc.set("format", "bird_prefab");
	doc.set("schema_version", m_schemaVersion);
	doc.set("prefab_asset_id", m_assetId);
	doc.set("root", m_rootEntity);
	return doc;
}

std::string Prefab::toJsonString(int indent) const {
	return toJson().dump(indent);
}

UUID Prefab::getRootLocalUUID() const {
	if (!m_rootEntity.isObject()) return UUID::nil();
	if (const JsonValue* uuidVal = m_rootEntity.find("uuid");
		uuidVal && uuidVal->isString()) {
		if (auto parsed = UUID::tryParse(uuidVal->asString())) {
			return *parsed;
		}
	}
	return UUID::nil();
}

std::vector<UUID> Prefab::collectLocalUUIDs() const {
	std::vector<UUID> result;
	collectLocalUUIDsRecursive(m_rootEntity, result);
	return result;
}

}  // namespace bird
