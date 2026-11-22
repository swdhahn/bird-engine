#include "ComponentRegistry.h"

#include <algorithm>

namespace bird {

ComponentRegistry& ComponentRegistry::instance() {
	static ComponentRegistry s_registry;
	return s_registry;
}

void ComponentRegistry::registerDescriptor(ComponentDescriptor descriptor) {
	const std::string name = descriptor.typeName;
	const std::type_index ti = descriptor.typeIndex;
	m_byName[name] = std::move(descriptor);
	if (ti != std::type_index(typeid(void))) {
		m_typeToName[ti] = name;
	}
}

void ComponentRegistry::registerTypeAlias(
	std::type_index aliasType, const std::string& canonicalTypeName) {
	m_typeToName[aliasType] = canonicalTypeName;
}

void ComponentRegistry::unregisterComponent(const std::string& typeName) {
	auto it = m_byName.find(typeName);
	if (it == m_byName.end()) return;
	for (auto tit = m_typeToName.begin(); tit != m_typeToName.end();) {
		if (tit->second == typeName) {
			tit = m_typeToName.erase(tit);
		} else {
			++tit;
		}
	}
	m_byName.erase(it);
}

bool ComponentRegistry::isRegistered(const std::string& typeName) const {
	return m_byName.find(typeName) != m_byName.end();
}

const ComponentDescriptor* ComponentRegistry::findByName(
	const std::string& typeName) const {
	auto it = m_byName.find(typeName);
	return it != m_byName.end() ? &it->second : nullptr;
}

const ComponentDescriptor* ComponentRegistry::findByType(
	std::type_index type) const {
	auto it = m_typeToName.find(type);
	if (it == m_typeToName.end()) return nullptr;
	return findByName(it->second);
}

const ComponentDescriptor* ComponentRegistry::findForComponent(
	const Component& comp) const {
	return findByType(std::type_index(typeid(comp)));
}

bool ComponentRegistry::serializeComponent(const Component& comp,
										   Archive& ar) const {
	const ComponentDescriptor* desc = findForComponent(comp);
	if (!desc) return false;

	for (const auto& fd : desc->fields) {
		if (fd.writeToArchive) {
			fd.writeToArchive(comp, ar);
		}
	}
	if (desc->customSerialize) {
		desc->customSerialize(comp, ar);
	}
	return true;
}

_own<Component> ComponentRegistry::deserializeComponent(
	const std::string& typeName, Archive& ar) const {
	const ComponentDescriptor* desc = findByName(typeName);
	if (!desc) return nullptr;

	_own<Component> comp;
	if (desc->factoryFromArchive) {
		comp = desc->factoryFromArchive(ar);
	} else if (desc->factory) {
		comp = desc->factory();
	}
	if (!comp) return nullptr;

	// Only apply T-typed field/custom deserializers when the factory produced
	// an instance matching desc->typeIndex (e.g. a live Dart factory may have
	// already constructed and populated a different Component subclass).
	if (desc->typeIndex == std::type_index(typeid(void)) ||
		std::type_index(typeid(*comp)) == desc->typeIndex) {
		for (const auto& fd : desc->fields) {
			if (fd.readFromArchive) {
				fd.readFromArchive(*comp, ar);
			}
		}
		if (desc->customDeserialize) {
			desc->customDeserialize(*comp, ar);
		}
	}
	return comp;
}

std::optional<SerializedValue> ComponentRegistry::getFieldValue(
	const Component& comp, const std::string& fieldName) const {
	const ComponentDescriptor* desc = findForComponent(comp);
	if (!desc) return std::nullopt;

	if (const FieldDescriptor* fd = desc->findField(fieldName)) {
		if (fd->getValue) return fd->getValue(comp);
	}
	if (desc->dynamicGetField) {
		return desc->dynamicGetField(comp, fieldName);
	}
	return std::nullopt;
}

bool ComponentRegistry::applyFieldOverride(
	Component& comp, const std::string& fieldName, const SerializedValue& value,
	const SerializationContext* ctx) const {
	const ComponentDescriptor* desc = findForComponent(comp);
	if (!desc) return false;

	if (const FieldDescriptor* fd = desc->findField(fieldName)) {
		if (fd->setValue) return fd->setValue(comp, value, ctx);
	}
	if (desc->dynamicSetField) {
		return desc->dynamicSetField(comp, fieldName, value, ctx);
	}
	return false;
}

std::vector<std::pair<std::string, SerializedValue>>
ComponentRegistry::enumerateFields(const Component& comp) const {
	std::vector<std::pair<std::string, SerializedValue>> result;
	const ComponentDescriptor* desc = findForComponent(comp);
	if (!desc) return result;

	result.reserve(desc->fields.size());
	for (const auto& fd : desc->fields) {
		if (fd.getValue) {
			result.emplace_back(fd.name, fd.getValue(comp));
		}
	}
	if (desc->dynamicEnumerateFields) {
		auto dyn = desc->dynamicEnumerateFields(comp);
		for (auto& kv : dyn) {
			result.push_back(std::move(kv));
		}
	}
	return result;
}

std::vector<std::string> ComponentRegistry::getRegisteredTypeNames() const {
	std::vector<std::string> names;
	names.reserve(m_byName.size());
	for (const auto& kv : m_byName) {
		names.push_back(kv.first);
	}
	std::sort(names.begin(), names.end());
	return names;
}

}  // namespace bird
