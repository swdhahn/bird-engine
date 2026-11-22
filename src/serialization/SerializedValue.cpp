#include "SerializedValue.h"

#include <cmath>

namespace bird {

SerializedValueType SerializedValue::type() const noexcept {
	return static_cast<SerializedValueType>(m_data.index());
}

bool SerializedValue::asBool(bool fallback) const {
	if (auto* p = std::get_if<bool>(&m_data)) return *p;
	return fallback;
}

int64_t SerializedValue::asInt(int64_t fallback) const {
	if (auto* p = std::get_if<int64_t>(&m_data)) return *p;
	if (auto* p = std::get_if<double>(&m_data)) return static_cast<int64_t>(*p);
	return fallback;
}

double SerializedValue::asDouble(double fallback) const {
	if (auto* p = std::get_if<double>(&m_data)) return *p;
	if (auto* p = std::get_if<int64_t>(&m_data)) return static_cast<double>(*p);
	return fallback;
}

float SerializedValue::asFloat(float fallback) const {
	return static_cast<float>(asDouble(static_cast<double>(fallback)));
}

const std::string& SerializedValue::asString() const {
	static const std::string kEmpty;
	if (auto* p = std::get_if<std::string>(&m_data)) return *p;
	return kEmpty;
}

Vector2 SerializedValue::asVec2(const Vector2& fallback) const {
	if (auto* p = std::get_if<Vector2>(&m_data)) return *p;
	if (auto* arr = std::get_if<Array>(&m_data); arr && arr->size() == 2) {
		return Vector2((*arr)[0].asFloat(), (*arr)[1].asFloat());
	}
	return fallback;
}

Vector3 SerializedValue::asVec3(const Vector3& fallback) const {
	if (auto* p = std::get_if<Vector3>(&m_data)) return *p;
	if (auto* arr = std::get_if<Array>(&m_data); arr && arr->size() == 3) {
		return Vector3((*arr)[0].asFloat(), (*arr)[1].asFloat(),
					   (*arr)[2].asFloat());
	}
	return fallback;
}

Vector4 SerializedValue::asVec4(const Vector4& fallback) const {
	if (auto* p = std::get_if<Vector4>(&m_data)) return *p;
	if (auto* arr = std::get_if<Array>(&m_data); arr && arr->size() == 4) {
		return Vector4((*arr)[0].asFloat(), (*arr)[1].asFloat(),
					   (*arr)[2].asFloat(), (*arr)[3].asFloat());
	}
	return fallback;
}

Quaternion SerializedValue::asQuat(const Quaternion& fallback) const {
	if (auto* p = std::get_if<Quaternion>(&m_data)) return *p;
	if (auto* arr = std::get_if<Array>(&m_data); arr && arr->size() == 4) {
		// Stored as [x, y, z, w] in JSON; glm::quat constructor takes (w, x, y, z).
		return Quaternion((*arr)[3].asFloat(), (*arr)[0].asFloat(),
						  (*arr)[1].asFloat(), (*arr)[2].asFloat());
	}
	return fallback;
}

UUID SerializedValue::asUuid(UUID fallback) const {
	if (auto* p = std::get_if<UUID>(&m_data)) return *p;
	if (auto* p = std::get_if<EntityRef>(&m_data)) return p->uuid;
	if (auto* p = std::get_if<std::string>(&m_data)) {
		if (auto parsed = UUID::tryParse(*p)) return *parsed;
	}
	return fallback;
}

EntityRef SerializedValue::asEntityRef(EntityRef fallback) const {
	if (auto* p = std::get_if<EntityRef>(&m_data)) return *p;
	if (auto* p = std::get_if<UUID>(&m_data)) return EntityRef(*p);
	if (auto* p = std::get_if<std::string>(&m_data)) {
		if (auto parsed = UUID::tryParse(*p)) return EntityRef(*parsed);
	}
	return fallback;
}

const SerializedValue::Array& SerializedValue::asArray() const {
	static const Array kEmpty;
	if (auto* p = std::get_if<Array>(&m_data)) return *p;
	return kEmpty;
}

bool SerializedValue::approxEquals(const SerializedValue& other,
								   double epsilon) const {
	if (isNumber() && other.isNumber()) {
		return std::abs(asDouble() - other.asDouble()) <= epsilon;
	}
	if (type() != other.type()) return false;

	switch (type()) {
		case SerializedValueType::Null:
			return true;
		case SerializedValueType::Bool:
			return asBool() == other.asBool();
		case SerializedValueType::Int:
			return asInt() == other.asInt();
		case SerializedValueType::Double:
			return std::abs(asDouble() - other.asDouble()) <= epsilon;
		case SerializedValueType::String:
			return asString() == other.asString();
		case SerializedValueType::Vec2: {
			const Vector2 a = asVec2();
			const Vector2 b = other.asVec2();
			return std::abs(a.x - b.x) <= epsilon &&
				   std::abs(a.y - b.y) <= epsilon;
		}
		case SerializedValueType::Vec3: {
			const Vector3 a = asVec3();
			const Vector3 b = other.asVec3();
			return std::abs(a.x - b.x) <= epsilon &&
				   std::abs(a.y - b.y) <= epsilon &&
				   std::abs(a.z - b.z) <= epsilon;
		}
		case SerializedValueType::Vec4: {
			const Vector4 a = asVec4();
			const Vector4 b = other.asVec4();
			return std::abs(a.x - b.x) <= epsilon &&
				   std::abs(a.y - b.y) <= epsilon &&
				   std::abs(a.z - b.z) <= epsilon &&
				   std::abs(a.w - b.w) <= epsilon;
		}
		case SerializedValueType::Quat: {
			const Quaternion a = asQuat();
			const Quaternion b = other.asQuat();
			// q and -q represent the same orientation, compare both signs.
			const bool sameSign = std::abs(a.x - b.x) <= epsilon &&
								  std::abs(a.y - b.y) <= epsilon &&
								  std::abs(a.z - b.z) <= epsilon &&
								  std::abs(a.w - b.w) <= epsilon;
			const bool oppSign = std::abs(a.x + b.x) <= epsilon &&
								 std::abs(a.y + b.y) <= epsilon &&
								 std::abs(a.z + b.z) <= epsilon &&
								 std::abs(a.w + b.w) <= epsilon;
			return sameSign || oppSign;
		}
		case SerializedValueType::Uuid:
			return asUuid() == other.asUuid();
		case SerializedValueType::EntityRef:
			return asEntityRef() == other.asEntityRef();
		case SerializedValueType::Array: {
			const auto& a = asArray();
			const auto& b = other.asArray();
			if (a.size() != b.size()) return false;
			for (size_t i = 0; i < a.size(); ++i) {
				if (!a[i].approxEquals(b[i], epsilon)) return false;
			}
			return true;
		}
	}
	return false;
}

bool SerializedValue::operator==(const SerializedValue& other) const {
	return m_data == other.m_data;
}

}  // namespace bird
