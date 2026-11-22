#include "JsonArchive.h"

namespace bird {

JsonValue serializedValueToJson(const SerializedValue& value) {
	switch (value.type()) {
		case SerializedValueType::Null:
			return JsonValue(nullptr);
		case SerializedValueType::Bool:
			return JsonValue(value.asBool());
		case SerializedValueType::Int:
			return JsonValue(value.asInt());
		case SerializedValueType::Double:
			return JsonValue(value.asDouble());
		case SerializedValueType::String:
			return JsonValue(value.asString());
		case SerializedValueType::Vec2: {
			const Vector2 v = value.asVec2();
			JsonValue obj = JsonValue::object();
			obj.set("$type", "vec2");
			JsonValue arr = JsonValue::array();
			arr.pushBack(static_cast<double>(v.x));
			arr.pushBack(static_cast<double>(v.y));
			obj.set("v", std::move(arr));
			return obj;
		}
		case SerializedValueType::Vec3: {
			const Vector3 v = value.asVec3();
			JsonValue obj = JsonValue::object();
			obj.set("$type", "vec3");
			JsonValue arr = JsonValue::array();
			arr.pushBack(static_cast<double>(v.x));
			arr.pushBack(static_cast<double>(v.y));
			arr.pushBack(static_cast<double>(v.z));
			obj.set("v", std::move(arr));
			return obj;
		}
		case SerializedValueType::Vec4: {
			const Vector4 v = value.asVec4();
			JsonValue obj = JsonValue::object();
			obj.set("$type", "vec4");
			JsonValue arr = JsonValue::array();
			arr.pushBack(static_cast<double>(v.x));
			arr.pushBack(static_cast<double>(v.y));
			arr.pushBack(static_cast<double>(v.z));
			arr.pushBack(static_cast<double>(v.w));
			obj.set("v", std::move(arr));
			return obj;
		}
		case SerializedValueType::Quat: {
			const Quaternion q = value.asQuat();
			JsonValue obj = JsonValue::object();
			obj.set("$type", "quat");
			JsonValue arr = JsonValue::array();
			arr.pushBack(static_cast<double>(q.x));
			arr.pushBack(static_cast<double>(q.y));
			arr.pushBack(static_cast<double>(q.z));
			arr.pushBack(static_cast<double>(q.w));
			obj.set("v", std::move(arr));
			return obj;
		}
		case SerializedValueType::Uuid: {
			JsonValue obj = JsonValue::object();
			obj.set("$type", "uuid");
			obj.set("v", value.asUuid().toString());
			return obj;
		}
		case SerializedValueType::EntityRef: {
			JsonValue obj = JsonValue::object();
			obj.set("$type", "entity_ref");
			obj.set("v", value.asEntityRef().uuid.toString());
			return obj;
		}
		case SerializedValueType::Array: {
			JsonValue arr = JsonValue::array();
			for (const auto& elem : value.asArray()) {
				arr.pushBack(serializedValueToJson(elem));
			}
			return arr;
		}
	}
	return JsonValue(nullptr);
}

SerializedValue jsonToSerializedValue(const JsonValue& json,
									  const SerializationContext* ctx) {
	switch (json.type()) {
		case JsonType::Null:
			return SerializedValue(nullptr);
		case JsonType::Bool:
			return SerializedValue(json.asBool());
		case JsonType::Int:
			return SerializedValue(json.asInt());
		case JsonType::Double:
			return SerializedValue(json.asDouble());
		case JsonType::String:
			return SerializedValue(json.asString());
		case JsonType::Array: {
			SerializedValue::Array arr;
			arr.reserve(json.asArray().size());
			for (const auto& elem : json.asArray()) {
				arr.push_back(jsonToSerializedValue(elem, ctx));
			}
			return SerializedValue(std::move(arr));
		}
		case JsonType::Object: {
			const JsonValue* tag = json.find("$type");
			const JsonValue* val = json.find("v");
			if (tag && tag->isString() && val) {
				const std::string& t = tag->asString();
				if (t == "vec2" && val->isArray() && val->size() == 2) {
					const auto& a = val->asArray();
					return SerializedValue(
						Vector2(static_cast<float>(a[0].asDouble()),
								static_cast<float>(a[1].asDouble())));
				}
				if (t == "vec3" && val->isArray() && val->size() == 3) {
					const auto& a = val->asArray();
					return SerializedValue(
						Vector3(static_cast<float>(a[0].asDouble()),
								static_cast<float>(a[1].asDouble()),
								static_cast<float>(a[2].asDouble())));
				}
				if (t == "vec4" && val->isArray() && val->size() == 4) {
					const auto& a = val->asArray();
					return SerializedValue(
						Vector4(static_cast<float>(a[0].asDouble()),
								static_cast<float>(a[1].asDouble()),
								static_cast<float>(a[2].asDouble()),
								static_cast<float>(a[3].asDouble())));
				}
				if (t == "quat" && val->isArray() && val->size() == 4) {
					const auto& a = val->asArray();
					return SerializedValue(
						Quaternion(static_cast<float>(a[3].asDouble()),
								   static_cast<float>(a[0].asDouble()),
								   static_cast<float>(a[1].asDouble()),
								   static_cast<float>(a[2].asDouble())));
				}
				if (t == "uuid" && val->isString()) {
					if (auto parsed = UUID::tryParse(val->asString())) {
						return SerializedValue(*parsed);
					}
					return SerializedValue(UUID::nil());
				}
				if (t == "entity_ref" && val->isString()) {
					EntityRef ref;
					if (auto parsed = UUID::tryParse(val->asString())) {
						ref.uuid = *parsed;
					}
					if (ctx) ref = ctx->remapEntityRef(ref);
					return SerializedValue(ref);
				}
			}
			return SerializedValue(nullptr);
		}
	}
	return SerializedValue(nullptr);
}

// ------------------------------------------------------------ JsonWriteArchive

JsonWriteArchive::JsonWriteArchive() : m_root(JsonValue::object()) {
	m_stack.push_back(&m_root);
}

JsonValue& JsonWriteArchive::current() {
	return *m_stack.back();
}

void JsonWriteArchive::writeKeyValue(const char* name, JsonValue val) {
	JsonValue& cur = current();
	if (cur.isObject()) {
		cur.set(name ? name : "", std::move(val));
	} else if (cur.isArray()) {
		cur.pushBack(std::move(val));
	}
}

bool JsonWriteArchive::beginObject(const char* name) {
	JsonValue& cur = current();
	if (cur.isObject()) {
		cur.set(name ? name : "", JsonValue::object());
		m_stack.push_back(cur.find(name ? name : ""));
	} else if (cur.isArray()) {
		cur.pushBack(JsonValue::object());
		m_stack.push_back(&cur.asArray().back());
	}
	return true;
}

void JsonWriteArchive::endObject() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonWriteArchive::beginArray(const char* name, size_t& /*count*/) {
	JsonValue& cur = current();
	if (cur.isObject()) {
		cur.set(name ? name : "", JsonValue::array());
		m_stack.push_back(cur.find(name ? name : ""));
	} else if (cur.isArray()) {
		cur.pushBack(JsonValue::array());
		m_stack.push_back(&cur.asArray().back());
	}
	return true;
}

void JsonWriteArchive::endArray() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonWriteArchive::beginArrayElement(size_t /*index*/) {
	JsonValue& cur = current();
	cur.pushBack(JsonValue::object());
	m_stack.push_back(&cur.asArray().back());
	return true;
}

void JsonWriteArchive::endArrayElement() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonWriteArchive::field(const char* name, bool& value) {
	writeKeyValue(name, JsonValue(value));
	return true;
}

bool JsonWriteArchive::field(const char* name, int32_t& value) {
	writeKeyValue(name, JsonValue(static_cast<int64_t>(value)));
	return true;
}

bool JsonWriteArchive::field(const char* name, uint32_t& value) {
	writeKeyValue(name, JsonValue(static_cast<int64_t>(value)));
	return true;
}

bool JsonWriteArchive::field(const char* name, int64_t& value) {
	writeKeyValue(name, JsonValue(value));
	return true;
}

bool JsonWriteArchive::field(const char* name, uint64_t& value) {
	writeKeyValue(name, JsonValue(static_cast<int64_t>(value)));
	return true;
}

bool JsonWriteArchive::field(const char* name, float& value) {
	writeKeyValue(name, JsonValue(static_cast<double>(value)));
	return true;
}

bool JsonWriteArchive::field(const char* name, double& value) {
	writeKeyValue(name, JsonValue(value));
	return true;
}

bool JsonWriteArchive::field(const char* name, std::string& value) {
	writeKeyValue(name, JsonValue(value));
	return true;
}

bool JsonWriteArchive::field(const char* name, UUID& value) {
	writeKeyValue(name, JsonValue(value.toString()));
	return true;
}

bool JsonWriteArchive::field(const char* name, Vector2& value) {
	JsonValue arr = JsonValue::array();
	arr.pushBack(static_cast<double>(value.x));
	arr.pushBack(static_cast<double>(value.y));
	writeKeyValue(name, std::move(arr));
	return true;
}

bool JsonWriteArchive::field(const char* name, Vector3& value) {
	JsonValue arr = JsonValue::array();
	arr.pushBack(static_cast<double>(value.x));
	arr.pushBack(static_cast<double>(value.y));
	arr.pushBack(static_cast<double>(value.z));
	writeKeyValue(name, std::move(arr));
	return true;
}

bool JsonWriteArchive::field(const char* name, Vector4& value) {
	JsonValue arr = JsonValue::array();
	arr.pushBack(static_cast<double>(value.x));
	arr.pushBack(static_cast<double>(value.y));
	arr.pushBack(static_cast<double>(value.z));
	arr.pushBack(static_cast<double>(value.w));
	writeKeyValue(name, std::move(arr));
	return true;
}

bool JsonWriteArchive::field(const char* name, Quaternion& value) {
	JsonValue arr = JsonValue::array();
	arr.pushBack(static_cast<double>(value.x));
	arr.pushBack(static_cast<double>(value.y));
	arr.pushBack(static_cast<double>(value.z));
	arr.pushBack(static_cast<double>(value.w));
	writeKeyValue(name, std::move(arr));
	return true;
}

bool JsonWriteArchive::field(const char* name, Matrix4& value) {
	JsonValue arr = JsonValue::array();
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			arr.pushBack(static_cast<double>(value[c][r]));
		}
	}
	writeKeyValue(name, std::move(arr));
	return true;
}

bool JsonWriteArchive::field(const char* name, EntityRef& value) {
	writeKeyValue(name, JsonValue(value.uuid.toString()));
	return true;
}

bool JsonWriteArchive::field(const char* name, SerializedValue& value) {
	writeKeyValue(name, serializedValueToJson(value));
	return true;
}

// ------------------------------------------------------------- JsonReadArchive

JsonReadArchive::JsonReadArchive(const JsonValue& root) {
	m_stack.push_back(&root);
}

const JsonValue* JsonReadArchive::current() const {
	return m_stack.empty() ? nullptr : m_stack.back();
}

const JsonValue* JsonReadArchive::findField(const char* name) const {
	const JsonValue* cur = current();
	if (!cur || !cur->isObject() || !name) return nullptr;
	return cur->find(name);
}

bool JsonReadArchive::hasField(const char* name) const {
	return findField(name) != nullptr;
}

bool JsonReadArchive::beginObject(const char* name) {
	const JsonValue* child = findField(name);
	if (!child || !child->isObject()) return false;
	m_stack.push_back(child);
	return true;
}

void JsonReadArchive::endObject() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonReadArchive::beginArray(const char* name, size_t& count) {
	const JsonValue* child = findField(name);
	if (!child || !child->isArray()) {
		count = 0;
		return false;
	}
	count = child->size();
	m_stack.push_back(child);
	return true;
}

void JsonReadArchive::endArray() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonReadArchive::beginArrayElement(size_t index) {
	const JsonValue* cur = current();
	if (!cur || !cur->isArray() || index >= cur->size()) return false;
	m_stack.push_back(&cur->asArray()[index]);
	return true;
}

void JsonReadArchive::endArrayElement() {
	if (m_stack.size() > 1) m_stack.pop_back();
}

bool JsonReadArchive::field(const char* name, bool& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isBool()) return false;
	value = v->asBool();
	return true;
}

bool JsonReadArchive::field(const char* name, int32_t& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = static_cast<int32_t>(v->asInt());
	return true;
}

bool JsonReadArchive::field(const char* name, uint32_t& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = static_cast<uint32_t>(v->asInt());
	return true;
}

bool JsonReadArchive::field(const char* name, int64_t& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = v->asInt();
	return true;
}

bool JsonReadArchive::field(const char* name, uint64_t& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = static_cast<uint64_t>(v->asInt());
	return true;
}

bool JsonReadArchive::field(const char* name, float& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = static_cast<float>(v->asDouble());
	return true;
}

bool JsonReadArchive::field(const char* name, double& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isNumber()) return false;
	value = v->asDouble();
	return true;
}

bool JsonReadArchive::field(const char* name, std::string& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isString()) return false;
	value = v->asString();
	return true;
}

bool JsonReadArchive::field(const char* name, UUID& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isString()) return false;
	auto parsed = UUID::tryParse(v->asString());
	if (!parsed) return false;
	value = *parsed;
	return true;
}

bool JsonReadArchive::field(const char* name, Vector2& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isArray() || v->size() != 2) return false;
	const auto& a = v->asArray();
	value = Vector2(static_cast<float>(a[0].asDouble()),
					static_cast<float>(a[1].asDouble()));
	return true;
}

bool JsonReadArchive::field(const char* name, Vector3& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isArray() || v->size() != 3) return false;
	const auto& a = v->asArray();
	value = Vector3(static_cast<float>(a[0].asDouble()),
					static_cast<float>(a[1].asDouble()),
					static_cast<float>(a[2].asDouble()));
	return true;
}

bool JsonReadArchive::field(const char* name, Vector4& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isArray() || v->size() != 4) return false;
	const auto& a = v->asArray();
	value = Vector4(static_cast<float>(a[0].asDouble()),
					static_cast<float>(a[1].asDouble()),
					static_cast<float>(a[2].asDouble()),
					static_cast<float>(a[3].asDouble()));
	return true;
}

bool JsonReadArchive::field(const char* name, Quaternion& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isArray() || v->size() != 4) return false;
	const auto& a = v->asArray();
	value = Quaternion(static_cast<float>(a[3].asDouble()),
					   static_cast<float>(a[0].asDouble()),
					   static_cast<float>(a[1].asDouble()),
					   static_cast<float>(a[2].asDouble()));
	return true;
}

bool JsonReadArchive::field(const char* name, Matrix4& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isArray() || v->size() != 16) return false;
	const auto& a = v->asArray();
	for (int c = 0; c < 4; ++c) {
		for (int r = 0; r < 4; ++r) {
			value[c][r] = static_cast<float>(a[static_cast<size_t>(c * 4 + r)].asDouble());
		}
	}
	return true;
}

bool JsonReadArchive::field(const char* name, EntityRef& value) {
	const JsonValue* v = findField(name);
	if (!v || !v->isString()) return false;
	auto parsed = UUID::tryParse(v->asString());
	if (!parsed) return false;
	EntityRef ref(*parsed);
	if (auto* ctx = context()) {
		ref = ctx->remapEntityRef(ref);
	}
	value = ref;
	return true;
}

bool JsonReadArchive::field(const char* name, SerializedValue& value) {
	const JsonValue* v = findField(name);
	if (!v) return false;
	value = jsonToSerializedValue(*v, context());
	return true;
}

}  // namespace bird
