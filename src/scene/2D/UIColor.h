#ifndef BIRD_UICOLOR_H
#define BIRD_UICOLOR_H

#include <optional>
#include <string>

#include "../../util/Math.h"

namespace bird::color {

/** Hue in degrees [0, 360), saturation and value in [0, 1]. */
struct HSV {
	float h = 0.0f;
	float s = 0.0f;
	float v = 0.0f;
};

/** @return RGB in [0, 1]; hue wraps, s and v are clamped. */
Vector3 hsvToRgb(const HSV& hsv);
/**
 * @param fallbackHue - hue to report for greys (s == 0) or black (v == 0),
 *        where hue is undefined; keeps a picker's hue stable.
 */
HSV rgbToHsv(const Vector3& rgb, float fallbackHue = 0.0f);

/**
 * Formats a colour as "#RRGGBB", or "#RRGGBBAA" when alpha is below 1 or
 * includeAlpha is set. Channels are clamped and rounded.
 */
std::string toHex(const Vector4& rgba, bool includeAlpha = false);
/**
 * Parses "#RGB", "#RGBA", "#RRGGBB" or "#RRGGBBAA" (the '#' is optional,
 * hex digits are case-insensitive, surrounding spaces are ignored).
 * @return the colour, or nullopt for anything else.
 */
std::optional<Vector4> parseHex(const std::string& text);

}  // namespace bird::color

#endif	// !BIRD_UICOLOR_H
