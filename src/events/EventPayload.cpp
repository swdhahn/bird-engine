#include "EventPayload.h"

namespace bird {

EventField* EventPayload::findField(std::string_view key) {
	for (auto& field : m_fields) {
		if (field.key == key) return &field;
	}
	return nullptr;
}

const EventField* EventPayload::findField(std::string_view key) const {
	for (const auto& field : m_fields) {
		if (field.key == key) return &field;
	}
	return nullptr;
}

EventPayload& EventPayload::setField(std::string_view key, EventValue value) {
	if (EventField* existing = findField(key)) {
		existing->value = std::move(value);
	} else {
		m_fields.push_back(EventField{std::string(key), std::move(value)});
	}
	return *this;
}

EventPayload& EventPayload::set(std::string_view key, bool value) {
	return setField(key, value);
}

EventPayload& EventPayload::set(std::string_view key, int64_t value) {
	return setField(key, value);
}

EventPayload& EventPayload::set(std::string_view key, int value) {
	return setField(key, static_cast<int64_t>(value));
}

EventPayload& EventPayload::set(std::string_view key, uint64_t value) {
	return setField(key, static_cast<int64_t>(value));
}

EventPayload& EventPayload::set(std::string_view key, double value) {
	return setField(key, value);
}

EventPayload& EventPayload::set(std::string_view key, float value) {
	return setField(key, static_cast<double>(value));
}

EventPayload& EventPayload::set(std::string_view key, std::string value) {
	return setField(key, std::move(value));
}

EventPayload& EventPayload::set(std::string_view key, const char* value) {
	return setField(key, std::string(value ? value : ""));
}

EventPayload& EventPayload::set(std::string_view key, const Vector2& value) {
	return setField(key, value);
}

EventPayload& EventPayload::set(std::string_view key, const Vector3& value) {
	return setField(key, value);
}

EventPayload& EventPayload::set(std::string_view key, const Vector4& value) {
	return setField(key, value);
}

EventPayload& EventPayload::setNull(std::string_view key) {
	return setField(key, std::monostate{});
}

bool EventPayload::has(std::string_view key) const {
	return findField(key) != nullptr;
}

EventValueType EventPayload::valueTypeOf(const EventValue& value) {
	switch (value.index()) {
		case 1:
			return EventValueType::Bool;
		case 2:
			return EventValueType::Int;
		case 3:
			return EventValueType::Double;
		case 4:
			return EventValueType::String;
		case 5:
			return EventValueType::Vector2;
		case 6:
			return EventValueType::Vector3;
		case 7:
			return EventValueType::Vector4;
		default:
			return EventValueType::Null;
	}
}

EventValueType EventPayload::getType(std::string_view key) const {
	const EventField* field = findField(key);
	if (!field) return EventValueType::Null;
	return valueTypeOf(field->value);
}

bool EventPayload::getBool(std::string_view key, bool defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<bool>(&field->value)) return *val;
	return defaultValue;
}

int64_t EventPayload::getInt(std::string_view key, int64_t defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<int64_t>(&field->value)) return *val;
	return defaultValue;
}

double EventPayload::getDouble(std::string_view key,
							   double defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<double>(&field->value)) return *val;
	if (const auto* ival = std::get_if<int64_t>(&field->value)) {
		return static_cast<double>(*ival);
	}
	return defaultValue;
}

float EventPayload::getFloat(std::string_view key, float defaultValue) const {
	return static_cast<float>(getDouble(key, static_cast<double>(defaultValue)));
}

std::string EventPayload::getString(std::string_view key,
									const std::string& defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<std::string>(&field->value)) return *val;
	return defaultValue;
}

Vector2 EventPayload::getVector2(std::string_view key,
								 const Vector2& defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<Vector2>(&field->value)) return *val;
	return defaultValue;
}

Vector3 EventPayload::getVector3(std::string_view key,
								 const Vector3& defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<Vector3>(&field->value)) return *val;
	return defaultValue;
}

Vector4 EventPayload::getVector4(std::string_view key,
								 const Vector4& defaultValue) const {
	const EventField* field = findField(key);
	if (!field) return defaultValue;
	if (const auto* val = std::get_if<Vector4>(&field->value)) return *val;
	return defaultValue;
}

void EventPayload::clear() {
	m_fields.clear();
}

}  // namespace bird
