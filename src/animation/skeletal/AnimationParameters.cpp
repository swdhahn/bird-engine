#include "AnimationParameters.h"

namespace bird {

void AnimationParameters::setFloat(const std::string& name, float value) {
	Entry& entry = m_entries[name];
	entry.type = AnimationParameterType::Float;
	entry.floatValue = value;
}

float AnimationParameters::getFloat(const std::string& name,
									float defaultValue) const {
	const auto it = m_entries.find(name);
	if (it == m_entries.end() ||
		it->second.type != AnimationParameterType::Float) {
		return defaultValue;
	}
	return it->second.floatValue;
}

void AnimationParameters::setBool(const std::string& name, bool value) {
	Entry& entry = m_entries[name];
	entry.type = AnimationParameterType::Bool;
	entry.boolValue = value;
}

bool AnimationParameters::getBool(const std::string& name,
								  bool defaultValue) const {
	const auto it = m_entries.find(name);
	if (it == m_entries.end() ||
		it->second.type != AnimationParameterType::Bool) {
		return defaultValue;
	}
	return it->second.boolValue;
}

void AnimationParameters::setInt(const std::string& name, int32_t value) {
	Entry& entry = m_entries[name];
	entry.type = AnimationParameterType::Int;
	entry.intValue = value;
}

int32_t AnimationParameters::getInt(const std::string& name,
									int32_t defaultValue) const {
	const auto it = m_entries.find(name);
	if (it == m_entries.end() ||
		it->second.type != AnimationParameterType::Int) {
		return defaultValue;
	}
	return it->second.intValue;
}

void AnimationParameters::setTrigger(const std::string& name) {
	Entry& entry = m_entries[name];
	entry.type = AnimationParameterType::Trigger;
	entry.boolValue = true;
}

void AnimationParameters::resetTrigger(const std::string& name) {
	const auto it = m_entries.find(name);
	if (it != m_entries.end() &&
		it->second.type == AnimationParameterType::Trigger) {
		it->second.boolValue = false;
	}
}

bool AnimationParameters::isTriggerSet(const std::string& name) const {
	const auto it = m_entries.find(name);
	return it != m_entries.end() &&
		   it->second.type == AnimationParameterType::Trigger &&
		   it->second.boolValue;
}

bool AnimationParameters::consumeTrigger(const std::string& name) {
	const auto it = m_entries.find(name);
	if (it == m_entries.end() ||
		it->second.type != AnimationParameterType::Trigger ||
		!it->second.boolValue) {
		return false;
	}
	it->second.boolValue = false;
	return true;
}

bool AnimationParameters::hasParameter(const std::string& name) const {
	return m_entries.find(name) != m_entries.end();
}

bool AnimationParameters::tryGetType(const std::string& name,
									 AnimationParameterType& outType) const {
	const auto it = m_entries.find(name);
	if (it == m_entries.end()) return false;
	outType = it->second.type;
	return true;
}

void AnimationParameters::clear() {
	m_entries.clear();
}

}  // namespace bird
