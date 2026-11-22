#include "Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace bird {

namespace {

void appendEscapedString(std::string& out, const std::string& s) {
	out.push_back('"');
	for (unsigned char c : s) {
		switch (c) {
			case '"':
				out += "\\\"";
				break;
			case '\\':
				out += "\\\\";
				break;
			case '\b':
				out += "\\b";
				break;
			case '\f':
				out += "\\f";
				break;
			case '\n':
				out += "\\n";
				break;
			case '\r':
				out += "\\r";
				break;
			case '\t':
				out += "\\t";
				break;
			default:
				if (c < 0x20) {
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x",
								  static_cast<unsigned int>(c));
					out += buf;
				} else {
					out.push_back(static_cast<char>(c));
				}
				break;
		}
	}
	out.push_back('"');
}

void appendIndent(std::string& out, int depth, int indent) {
	if (indent > 0 && depth > 0) {
		out.append(static_cast<size_t>(depth * indent), ' ');
	}
}

void dumpImpl(const JsonValue& val, std::string& out, int indent, int depth) {
	switch (val.type()) {
		case JsonType::Null:
			out += "null";
			break;
		case JsonType::Bool:
			out += val.asBool() ? "true" : "false";
			break;
		case JsonType::Int:
			out += std::to_string(val.asInt());
			break;
		case JsonType::Double: {
			const double d = val.asDouble();
			if (!std::isfinite(d)) {
				out += "null";
				break;
			}
			std::ostringstream oss;
			oss << std::setprecision(15) << d;
			std::string num = oss.str();
			// Ensure floats retain a decimal indicator so they re-parse as Double.
			if (num.find('.') == std::string::npos &&
				num.find('e') == std::string::npos &&
				num.find('E') == std::string::npos) {
				num += ".0";
			}
			out += num;
			break;
		}
		case JsonType::String:
			appendEscapedString(out, val.asString());
			break;
		case JsonType::Array: {
			const auto& arr = val.asArray();
			if (arr.empty()) {
				out += "[]";
				break;
			}
			// Keep short arrays of primitives (e.g. Vec2/Vec3/Vec4/Quat) inline.
			bool allPrimitives = arr.size() <= 4;
			if (allPrimitives) {
				for (const auto& elem : arr) {
					if (elem.isArray() || elem.isObject()) {
						allPrimitives = false;
						break;
					}
				}
			}
			out.push_back('[');
			if (indent >= 0 && !allPrimitives) {
				out.push_back('\n');
			}
			for (size_t i = 0; i < arr.size(); ++i) {
				if (indent >= 0 && !allPrimitives) {
					appendIndent(out, depth + 1, indent);
				}
				dumpImpl(arr[i], out, indent, depth + 1);
				if (i + 1 < arr.size()) {
					out.push_back(',');
					if (indent >= 0 && allPrimitives) out.push_back(' ');
				}
				if (indent >= 0 && !allPrimitives) {
					out.push_back('\n');
				}
			}
			if (indent >= 0 && !allPrimitives) {
				appendIndent(out, depth, indent);
			}
			out.push_back(']');
			break;
		}
		case JsonType::Object: {
			const auto& obj = val.asObject();
			if (obj.empty()) {
				out += "{}";
				break;
			}
			out.push_back('{');
			if (indent >= 0) out.push_back('\n');
			for (size_t i = 0; i < obj.size(); ++i) {
				if (indent >= 0) appendIndent(out, depth + 1, indent);
				appendEscapedString(out, obj[i].first);
				out += (indent >= 0) ? ": " : ":";
				dumpImpl(obj[i].second, out, indent, depth + 1);
				if (i + 1 < obj.size()) out.push_back(',');
				if (indent >= 0) out.push_back('\n');
			}
			if (indent >= 0) appendIndent(out, depth, indent);
			out.push_back('}');
			break;
		}
	}
}

void encodeUtf8(std::string& out, uint32_t codepoint) {
	if (codepoint <= 0x7F) {
		out.push_back(static_cast<char>(codepoint));
	} else if (codepoint <= 0x7FF) {
		out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint <= 0xFFFF) {
		out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint <= 0x10FFFF) {
		out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	}
}

class JsonParser {
   public:
	explicit JsonParser(const std::string& text) : m_text(text) {}

	std::optional<JsonValue> parseRoot(std::string* errorOut) {
		skipWhitespace();
		auto value = parseValue();
		if (!value) {
			if (errorOut) *errorOut = m_error;
			return std::nullopt;
		}
		skipWhitespace();
		if (m_pos != m_text.size()) {
			setError("Unexpected trailing characters after JSON root value");
			if (errorOut) *errorOut = m_error;
			return std::nullopt;
		}
		return value;
	}

   private:
	static constexpr int kMaxDepth = 256;

	const std::string& m_text;
	size_t m_pos = 0;
	size_t m_line = 1;
	size_t m_col = 1;
	int m_depth = 0;
	std::string m_error;

	void setError(const std::string& msg) {
		if (m_error.empty()) {
			m_error = "JSON parse error at line " + std::to_string(m_line) +
					  ", column " + std::to_string(m_col) + ": " + msg;
		}
	}

	bool eof() const { return m_pos >= m_text.size(); }
	char peek() const { return eof() ? '\0' : m_text[m_pos]; }

	char advance() {
		if (eof()) return '\0';
		char c = m_text[m_pos++];
		if (c == '\n') {
			++m_line;
			m_col = 1;
		} else {
			++m_col;
		}
		return c;
	}

	void skipWhitespace() {
		while (!eof()) {
			char c = peek();
			if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
				advance();
			} else {
				break;
			}
		}
	}

	std::optional<JsonValue> parseValue() {
		skipWhitespace();
		if (eof()) {
			setError("Unexpected end of input");
			return std::nullopt;
		}
		char c = peek();
		if (c == '{') return parseObject();
		if (c == '[') return parseArray();
		if (c == '"') {
			auto s = parseString();
			if (!s) return std::nullopt;
			return JsonValue(std::move(*s));
		}
		if (c == 't') return parseLiteral("true", JsonValue(true));
		if (c == 'f') return parseLiteral("false", JsonValue(false));
		if (c == 'n') return parseLiteral("null", JsonValue(nullptr));
		if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();

		setError(std::string("Unexpected character '") + c + "'");
		return std::nullopt;
	}

	std::optional<JsonValue> parseLiteral(const char* lit, JsonValue value) {
		for (const char* p = lit; *p; ++p) {
			if (eof() || advance() != *p) {
				setError(std::string("Expected '") + lit + "'");
				return std::nullopt;
			}
		}
		return value;
	}

	std::optional<JsonValue> parseObject() {
		if (++m_depth > kMaxDepth) {
			setError("Maximum nesting depth exceeded");
			return std::nullopt;
		}
		advance();  // '{'
		JsonValue obj = JsonValue::object();
		skipWhitespace();
		if (peek() == '}') {
			advance();
			--m_depth;
			return obj;
		}
		while (true) {
			skipWhitespace();
			if (peek() != '"') {
				setError("Expected string key in object");
				return std::nullopt;
			}
			auto key = parseString();
			if (!key) return std::nullopt;

			skipWhitespace();
			if (peek() != ':') {
				setError("Expected ':' after object key");
				return std::nullopt;
			}
			advance();

			auto val = parseValue();
			if (!val) return std::nullopt;
			obj.set(*key, std::move(*val));

			skipWhitespace();
			if (peek() == ',') {
				advance();
				continue;
			}
			if (peek() == '}') {
				advance();
				--m_depth;
				return obj;
			}
			setError("Expected ',' or '}' in object");
			return std::nullopt;
		}
	}

	std::optional<JsonValue> parseArray() {
		if (++m_depth > kMaxDepth) {
			setError("Maximum nesting depth exceeded");
			return std::nullopt;
		}
		advance();  // '['
		JsonValue arr = JsonValue::array();
		skipWhitespace();
		if (peek() == ']') {
			advance();
			--m_depth;
			return arr;
		}
		while (true) {
			auto elem = parseValue();
			if (!elem) return std::nullopt;
			arr.pushBack(std::move(*elem));

			skipWhitespace();
			if (peek() == ',') {
				advance();
				continue;
			}
			if (peek() == ']') {
				advance();
				--m_depth;
				return arr;
			}
			setError("Expected ',' or ']' in array");
			return std::nullopt;
		}
	}

	bool parseHex4(uint16_t& out) {
		out = 0;
		for (int i = 0; i < 4; ++i) {
			if (eof()) {
				setError("Truncated \\uXXXX escape");
				return false;
			}
			char c = advance();
			uint16_t v = 0;
			if (c >= '0' && c <= '9')
				v = static_cast<uint16_t>(c - '0');
			else if (c >= 'a' && c <= 'f')
				v = static_cast<uint16_t>(10 + (c - 'a'));
			else if (c >= 'A' && c <= 'F')
				v = static_cast<uint16_t>(10 + (c - 'A'));
			else {
				setError("Invalid hex digit in \\uXXXX escape");
				return false;
			}
			out = static_cast<uint16_t>((out << 4) | v);
		}
		return true;
	}

	std::optional<std::string> parseString() {
		advance();  // '"'
		std::string result;
		while (!eof()) {
			char c = advance();
			if (c == '"') return result;
			if (static_cast<unsigned char>(c) < 0x20) {
				setError("Unescaped control character in string");
				return std::nullopt;
			}
			if (c == '\\') {
				if (eof()) {
					setError("Unterminated escape sequence");
					return std::nullopt;
				}
				char esc = advance();
				switch (esc) {
					case '"':
						result.push_back('"');
						break;
					case '\\':
						result.push_back('\\');
						break;
					case '/':
						result.push_back('/');
						break;
					case 'b':
						result.push_back('\b');
						break;
					case 'f':
						result.push_back('\f');
						break;
					case 'n':
						result.push_back('\n');
						break;
					case 'r':
						result.push_back('\r');
						break;
					case 't':
						result.push_back('\t');
						break;
					case 'u': {
						uint16_t code = 0;
						if (!parseHex4(code)) return std::nullopt;
						if (code >= 0xD800 && code <= 0xDBFF) {
							// High surrogate; expect low surrogate pair.
							if (m_pos + 1 < m_text.size() &&
								m_text[m_pos] == '\\' &&
								m_text[m_pos + 1] == 'u') {
								advance();
								advance();
								uint16_t low = 0;
								if (!parseHex4(low)) return std::nullopt;
								if (low >= 0xDC00 && low <= 0xDFFF) {
									uint32_t cp =
										0x10000 +
										((static_cast<uint32_t>(code - 0xD800) << 10) |
										 static_cast<uint32_t>(low - 0xDC00));
									encodeUtf8(result, cp);
									break;
								}
							}
							setError("Invalid UTF-16 surrogate pair");
							return std::nullopt;
						}
						encodeUtf8(result, code);
						break;
					}
					default:
						setError(std::string("Invalid escape '\\") + esc + "'");
						return std::nullopt;
				}
			} else {
				result.push_back(c);
			}
		}
		setError("Unterminated string literal");
		return std::nullopt;
	}

	std::optional<JsonValue> parseNumber() {
		const size_t start = m_pos;
		bool isFloat = false;

		if (peek() == '-') advance();
		if (eof()) {
			setError("Invalid number");
			return std::nullopt;
		}
		if (peek() == '0') {
			advance();
		} else if (peek() >= '1' && peek() <= '9') {
			while (!eof() && peek() >= '0' && peek() <= '9') advance();
		} else {
			setError("Invalid number");
			return std::nullopt;
		}

		if (peek() == '.') {
			isFloat = true;
			advance();
			if (eof() || peek() < '0' || peek() > '9') {
				setError("Expected digit after decimal point");
				return std::nullopt;
			}
			while (!eof() && peek() >= '0' && peek() <= '9') advance();
		}

		if (peek() == 'e' || peek() == 'E') {
			isFloat = true;
			advance();
			if (peek() == '+' || peek() == '-') advance();
			if (eof() || peek() < '0' || peek() > '9') {
				setError("Expected digit in exponent");
				return std::nullopt;
			}
			while (!eof() && peek() >= '0' && peek() <= '9') advance();
		}

		const std::string token = m_text.substr(start, m_pos - start);
		if (!isFloat) {
			try {
				size_t idx = 0;
				long long v = std::stoll(token, &idx, 10);
				if (idx == token.size()) {
					return JsonValue(static_cast<int64_t>(v));
				}
			} catch (...) {
				// Fall through to double if integer overflows int64_t.
			}
		}
		try {
			size_t idx = 0;
			double d = std::stod(token, &idx);
			if (idx == token.size()) {
				return JsonValue(d);
			}
		} catch (...) {
		}
		setError("Failed to parse numeric literal '" + token + "'");
		return std::nullopt;
	}
};

}  // namespace

bool JsonValue::asBool(bool fallback) const {
	if (auto* p = std::get_if<bool>(&m_data)) return *p;
	return fallback;
}

int64_t JsonValue::asInt(int64_t fallback) const {
	if (auto* p = std::get_if<int64_t>(&m_data)) return *p;
	if (auto* p = std::get_if<double>(&m_data)) return static_cast<int64_t>(*p);
	return fallback;
}

double JsonValue::asDouble(double fallback) const {
	if (auto* p = std::get_if<double>(&m_data)) return *p;
	if (auto* p = std::get_if<int64_t>(&m_data)) return static_cast<double>(*p);
	return fallback;
}

const std::string& JsonValue::asString() const {
	static const std::string kEmpty;
	if (auto* p = std::get_if<std::string>(&m_data)) return *p;
	return kEmpty;
}

const JsonValue::Array& JsonValue::asArray() const {
	static const Array kEmpty;
	if (auto* p = std::get_if<Array>(&m_data)) return *p;
	return kEmpty;
}

JsonValue::Array& JsonValue::asArray() {
	if (auto* p = std::get_if<Array>(&m_data)) return *p;
	throw std::logic_error("JsonValue::asArray: value is not an array");
}

const JsonValue::Object& JsonValue::asObject() const {
	static const Object kEmpty;
	if (auto* p = std::get_if<Object>(&m_data)) return *p;
	return kEmpty;
}

JsonValue::Object& JsonValue::asObject() {
	if (auto* p = std::get_if<Object>(&m_data)) return *p;
	throw std::logic_error("JsonValue::asObject: value is not an object");
}

size_t JsonValue::size() const noexcept {
	if (auto* p = std::get_if<Array>(&m_data)) return p->size();
	if (auto* p = std::get_if<Object>(&m_data)) return p->size();
	return 0;
}

void JsonValue::pushBack(JsonValue value) {
	asArray().push_back(std::move(value));
}

void JsonValue::set(const std::string& key, JsonValue value) {
	auto& obj = asObject();
	for (auto& kv : obj) {
		if (kv.first == key) {
			kv.second = std::move(value);
			return;
		}
	}
	obj.emplace_back(key, std::move(value));
}

bool JsonValue::contains(const std::string& key) const {
	return find(key) != nullptr;
}

const JsonValue* JsonValue::find(const std::string& key) const {
	if (auto* obj = std::get_if<Object>(&m_data)) {
		for (const auto& kv : *obj) {
			if (kv.first == key) return &kv.second;
		}
	}
	return nullptr;
}

JsonValue* JsonValue::find(const std::string& key) {
	if (auto* obj = std::get_if<Object>(&m_data)) {
		for (auto& kv : *obj) {
			if (kv.first == key) return &kv.second;
		}
	}
	return nullptr;
}

std::string JsonValue::dump(int indent) const {
	std::string out;
	out.reserve(256);
	dumpImpl(*this, out, indent, 0);
	return out;
}

std::optional<JsonValue> JsonValue::parse(const std::string& text,
										  std::string* errorOut) {
	JsonParser parser(text);
	return parser.parseRoot(errorOut);
}

bool JsonValue::operator==(const JsonValue& other) const {
	return m_data == other.m_data;
}

}  // namespace bird
