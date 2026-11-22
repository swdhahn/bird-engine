#include "DartScriptRecord.h"

namespace bird {

namespace {
DartScriptRecord::ScriptFactoryFn& globalScriptFactory() {
	static DartScriptRecord::ScriptFactoryFn s_factory;
	return s_factory;
}
}  // namespace

DartScriptRecord::DartScriptRecord(std::string scriptClass,
								   std::string scriptPath)
	: m_scriptClass(std::move(scriptClass)),
	  m_scriptPath(std::move(scriptPath)) {}

void DartScriptRecord::init() {
	++m_initCount;
	if (m_onInit) {
		m_onInit(*this);
	}
}

void DartScriptRecord::deinit() {
	if (m_onDeinit) {
		m_onDeinit(*this);
	}
}

void DartScriptRecord::setProperty(const std::string& name,
								   SerializedValue value) {
	m_properties[name] = std::move(value);
}

void DartScriptRecord::removeProperty(const std::string& name) {
	m_properties.erase(name);
}

bool DartScriptRecord::hasProperty(const std::string& name) const {
	return m_properties.find(name) != m_properties.end();
}

std::optional<SerializedValue> DartScriptRecord::getProperty(
	const std::string& name) const {
	auto it = m_properties.find(name);
	if (it == m_properties.end()) return std::nullopt;
	return it->second;
}

void DartScriptRecord::setScriptFactory(ScriptFactoryFn factory) {
	globalScriptFactory() = std::move(factory);
}

const DartScriptRecord::ScriptFactoryFn& DartScriptRecord::getScriptFactory() {
	return globalScriptFactory();
}

}  // namespace bird
