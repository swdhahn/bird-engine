#include "UIColor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace bird::color {

Vector3 hsvToRgb(const HSV& hsv) {
	const float h = std::fmod(std::fmod(hsv.h, 360.0f) + 360.0f, 360.0f) / 60.0f;
	const float s = std::clamp(hsv.s, 0.0f, 1.0f);
	const float v = std::clamp(hsv.v, 0.0f, 1.0f);
	const float c = v * s;
	const float x = c * (1.0f - std::abs(std::fmod(h, 2.0f) - 1.0f));
	const float m = v - c;
	Vector3 rgb;
	switch (static_cast<int>(h) % 6) {
		case 0: rgb = {c, x, 0}; break;
		case 1: rgb = {x, c, 0}; break;
		case 2: rgb = {0, c, x}; break;
		case 3: rgb = {0, x, c}; break;
		case 4: rgb = {x, 0, c}; break;
		default: rgb = {c, 0, x}; break;
	}
	return rgb + Vector3(m);
}

HSV rgbToHsv(const Vector3& rgbIn, float fallbackHue) {
	const Vector3 rgb = glm::clamp(rgbIn, Vector3(0.0f), Vector3(1.0f));
	const float maxC = std::max({rgb.r, rgb.g, rgb.b});
	const float minC = std::min({rgb.r, rgb.g, rgb.b});
	const float delta = maxC - minC;
	HSV hsv;
	hsv.v = maxC;
	hsv.s = maxC > 0.0f ? delta / maxC : 0.0f;
	if (delta <= 1e-6f) {
		hsv.h = fallbackHue;  // hue is undefined for greys
		return hsv;
	}
	float h;
	if (maxC == rgb.r) {
		h = std::fmod((rgb.g - rgb.b) / delta, 6.0f);
	} else if (maxC == rgb.g) {
		h = (rgb.b - rgb.r) / delta + 2.0f;
	} else {
		h = (rgb.r - rgb.g) / delta + 4.0f;
	}
	hsv.h = std::fmod(h * 60.0f + 360.0f, 360.0f);
	return hsv;
}

namespace {

int toByte(float channel) {
	return static_cast<int>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
}

int hexDigit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
}

}  // namespace

std::string toHex(const Vector4& rgba, bool includeAlpha) {
	char buffer[10];
	const int a = toByte(rgba.a);
	if (includeAlpha || a < 255) {
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X", toByte(rgba.r),
					  toByte(rgba.g), toByte(rgba.b), a);
	} else {
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", toByte(rgba.r),
					  toByte(rgba.g), toByte(rgba.b));
	}
	return buffer;
}

std::optional<Vector4> parseHex(const std::string& text) {
	size_t begin = text.find_first_not_of(" \t");
	size_t end = text.find_last_not_of(" \t");
	if (begin == std::string::npos) return std::nullopt;
	std::string digits = text.substr(begin, end - begin + 1);
	if (!digits.empty() && digits[0] == '#') digits.erase(0, 1);

	const size_t n = digits.size();
	if (n != 3 && n != 4 && n != 6 && n != 8) return std::nullopt;
	int values[8];
	for (size_t i = 0; i < n; i++) {
		values[i] = hexDigit(digits[i]);
		if (values[i] < 0) return std::nullopt;
	}
	Vector4 rgba(1.0f);
	const bool shortForm = n <= 4;	// #RGB(A): each digit is doubled
	const size_t channels = shortForm ? n : n / 2;
	for (size_t c = 0; c < channels; c++) {
		const int byte = shortForm ? values[c] * 17
								   : values[2 * c] * 16 + values[2 * c + 1];
		rgba[static_cast<int>(c)] = static_cast<float>(byte) / 255.0f;
	}
	return rgba;
}

}  // namespace bird::color
