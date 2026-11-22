#include "BuiltinComponents.h"

#include <mutex>

#include "../scene/components/LightComponent.h"
#include "../scene/components/MeshComponent.h"
#include "ComponentRegistry.h"
#include "DartScriptRecord.h"

#ifdef BIRD_ENABLE_DART
#include "../dart/DartScriptComponent.h"
#endif

namespace bird {

namespace {

void writeScriptProperties(const std::map<std::string, SerializedValue>& props,
						   Archive& ar) {
	size_t count = props.size();
	if (ar.beginArray("properties", count)) {
		size_t idx = 0;
		for (const auto& kv : props) {
			if (ar.beginArrayElement(idx++)) {
				std::string key = kv.first;
				SerializedValue val = kv.second;
				ar.field("name", key);
				ar.field("value", val);
				ar.endArrayElement();
			}
		}
		ar.endArray();
	}
}

void readScriptProperties(std::map<std::string, SerializedValue>& props,
						  Archive& ar) {
	props.clear();
	size_t count = 0;
	if (ar.beginArray("properties", count)) {
		for (size_t i = 0; i < count; ++i) {
			if (ar.beginArrayElement(i)) {
				std::string key;
				SerializedValue val;
				if (ar.field("name", key)) {
					ar.field("value", val);
					props[key] = std::move(val);
				}
				ar.endArrayElement();
			}
		}
		ar.endArray();
	}
}

}  // namespace

void registerBuiltinComponents() {
	static std::once_flag s_once;
	std::call_once(s_once, []() {
		auto& reg = ComponentRegistry::instance();

		// ------------------------------------------------------ LightComponent
		reg.registerComponent<LightComponent>(
			"LightComponent",
			[]() -> _own<Component> {
				return _own_mk<LightComponent>(LightType::Point);
			},
			[](ComponentMetaBuilder<LightComponent>& b) {
				b.factoryFromArchive([](Archive& ar) -> _own<Component> {
					int32_t rawType = static_cast<int32_t>(LightType::Point);
					ar.field("lightType", rawType);
					return _own_mk<LightComponent>(
						static_cast<LightType>(rawType));
				});
				b.enumField<LightType>(
					"lightType",
					[](const LightComponent& l) { return l.getType(); },
					[](LightComponent&, LightType) {
						// Immutable after construction; handled by factoryFromArchive.
					});
				b.field("color", &LightComponent::getColor,
						&LightComponent::setColor);
				b.field("intensity", &LightComponent::getIntensity,
						&LightComponent::setIntensity);
				b.field("range", &LightComponent::getRange,
						&LightComponent::setRange);
				b.field<float>(
					"innerConeAngle",
					[](const LightComponent& l) { return l.getInnerConeAngle(); },
					[](LightComponent& l, const float& v) {
						l.setConeAngles(v, l.getOuterConeAngle());
					});
				b.field<float>(
					"outerConeAngle",
					[](const LightComponent& l) { return l.getOuterConeAngle(); },
					[](LightComponent& l, const float& v) {
						l.setConeAngles(l.getInnerConeAngle(), v);
					});
				b.field("castsShadows", &LightComponent::getCastsShadows,
						&LightComponent::setCastsShadows);
				b.field("shadowDepthBias", &LightComponent::getShadowDepthBias,
						&LightComponent::setShadowDepthBias);
				b.field("shadowNormalBias", &LightComponent::getShadowNormalBias,
						&LightComponent::setShadowNormalBias);
			});

		// ------------------------------------------------------- MeshComponent
		reg.registerComponent<MeshComponent>(
			"MeshComponent",
			[]() -> _own<Component> {
				return _own_mk<MeshComponent>(std::vector<_ref<Mesh>>{});
			},
			[](ComponentMetaBuilder<MeshComponent>& b) {
				b.factoryFromArchive([](Archive& ar) -> _own<Component> {
					std::string assetPath;
					ar.field("assetPath", assetPath);
					if (!assetPath.empty() && RESOURCE_MANAGER != nullptr) {
						return _own_mk<MeshComponent>(assetPath);
					}
					auto comp = _own_mk<MeshComponent>(std::vector<_ref<Mesh>>{});
					comp->setAssetPath(assetPath);
					return comp;
				});
				b.field("assetPath", &MeshComponent::getAssetPath,
						&MeshComponent::setAssetPath);
			});

		// --------------------------------------------------------- DartScript
		reg.registerComponent<DartScriptRecord>(
			"DartScript",
			[]() -> _own<Component> { return _own_mk<DartScriptRecord>(); },
			[](ComponentMetaBuilder<DartScriptRecord>& b) {
				b.field("scriptClass", &DartScriptRecord::getScriptClass,
						&DartScriptRecord::setScriptClass);
				b.field("scriptPath", &DartScriptRecord::getScriptPath,
						&DartScriptRecord::setScriptPath);
				b.factoryFromArchive([](Archive& ar) -> _own<Component> {
					if (const auto& factory = DartScriptRecord::getScriptFactory()) {
						std::string scriptClass;
						std::string scriptPath;
						ar.field("scriptClass", scriptClass);
						ar.field("scriptPath", scriptPath);
						std::map<std::string, SerializedValue> props;
						readScriptProperties(props, ar);
						if (auto live = factory(scriptClass, scriptPath, props)) {
							return live;
						}
					}
					return _own_mk<DartScriptRecord>();
				});
				b.customSerializer(
					[](const DartScriptRecord& rec, Archive& ar) {
						writeScriptProperties(rec.getProperties(), ar);
					},
					[](DartScriptRecord& rec, Archive& ar) {
						readScriptProperties(rec.getProperties(), ar);
					});
				b.dynamicFields(
					[](const DartScriptRecord& rec,
					   const std::string& name) -> std::optional<SerializedValue> {
						return rec.getProperty(name);
					},
					[](DartScriptRecord& rec, const std::string& name,
					   const SerializedValue& val,
					   const SerializationContext* ctx) -> bool {
						SerializedValue applied = val;
						if (applied.isEntityRef() && ctx) {
							applied = SerializedValue(
								ctx->remapEntityRef(applied.asEntityRef()));
						}
						rec.setProperty(name, std::move(applied));
						return true;
					},
					[](const DartScriptRecord& rec) {
						std::vector<std::pair<std::string, SerializedValue>> out;
						out.reserve(rec.getProperties().size());
						for (const auto& kv : rec.getProperties()) {
							out.emplace_back(kv.first, kv.second);
						}
						return out;
					});
			});

#ifdef BIRD_ENABLE_DART
		// Register live DartScriptComponent serialization alongside DartScriptRecord.
		reg.registerComponent<DartScriptComponent>(
			"DartScriptComponent",
			[]() -> _own<Component> {
				return _own_mk<DartScriptComponent>(nullptr, nullptr, nullptr,
													nullptr);
			},
			[](ComponentMetaBuilder<DartScriptComponent>& b) {
				b.field("scriptClass", &DartScriptComponent::getScriptClass,
						&DartScriptComponent::setScriptClass);
				b.field("scriptPath", &DartScriptComponent::getScriptPath,
						&DartScriptComponent::setScriptPath);
				b.customSerializer(
					[](const DartScriptComponent& comp, Archive& ar) {
						std::map<std::string, SerializedValue> ordered(
							comp.getProperties().begin(),
							comp.getProperties().end());
						writeScriptProperties(ordered, ar);
					},
					[](DartScriptComponent& comp, Archive& ar) {
						std::map<std::string, SerializedValue> ordered;
						readScriptProperties(ordered, ar);
						comp.getProperties().clear();
						for (auto& kv : ordered) {
							comp.setProperty(kv.first, std::move(kv.second));
						}
					});
				b.dynamicFields(
					[](const DartScriptComponent& comp,
					   const std::string& name) -> std::optional<SerializedValue> {
						auto it = comp.getProperties().find(name);
						if (it == comp.getProperties().end()) return std::nullopt;
						return it->second;
					},
					[](DartScriptComponent& comp, const std::string& name,
					   const SerializedValue& val,
					   const SerializationContext* ctx) -> bool {
						SerializedValue applied = val;
						if (applied.isEntityRef() && ctx) {
							applied = SerializedValue(
								ctx->remapEntityRef(applied.asEntityRef()));
						}
						comp.setProperty(name, std::move(applied));
						return true;
					},
					[](const DartScriptComponent& comp) {
						std::vector<std::pair<std::string, SerializedValue>> out;
						out.reserve(comp.getProperties().size());
						for (const auto& kv : comp.getProperties()) {
							out.emplace_back(kv.first, kv.second);
						}
						return out;
					});
			});
#endif
	});
}

}  // namespace bird
