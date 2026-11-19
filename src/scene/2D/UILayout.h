#ifndef BIRD_UILAYOUT_H
#define BIRD_UILAYOUT_H

#include <algorithm>
#include <limits>

#include "../../util/Math.h"

namespace bird {

/** Space around (margin) or inside (padding) a UI element, in pixels. */
struct UIInsets {
	float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;

	static UIInsets all(float value) { return {value, value, value, value}; }
	static UIInsets symmetric(float horizontal, float vertical) {
		return {horizontal, vertical, horizontal, vertical};
	}
	float horizontal() const { return left + right; }
	float vertical() const { return top + bottom; }
	Vector2 total() const { return Vector2(horizontal(), vertical()); }
	Vector2 start() const { return Vector2(left, top); }
};

/**
 * How an element is sized along one axis.
 * - Fixed: exactly `value` pixels.
 * - Fit: as large as its content (plus padding).
 * - Fill: all available space; inside a UIStack, remaining main-axis space
 *   is shared between Fill children in proportion to `value` (the weight).
 */
enum class UISizeMode : uint8_t { Fixed, Fit, Fill };

struct UISizeRule {
	UISizeMode mode = UISizeMode::Fit;
	float value = 0.0f;
	float min = 0.0f;
	float max = std::numeric_limits<float>::infinity();

	static UISizeRule fixed(float pixels) {
		return {UISizeMode::Fixed, pixels};
	}
	static UISizeRule fit() { return {UISizeMode::Fit, 0.0f}; }
	static UISizeRule fill(float weight = 1.0f) {
		return {UISizeMode::Fill, weight};
	}
	float clamp(float size) const { return std::clamp(size, min, max); }
};

enum class UIAxis : uint8_t { Horizontal, Vertical };
/** Main-axis distribution of children in a UIStack. */
enum class UIJustify : uint8_t { Start, Center, End, SpaceBetween };
/** Cross-axis placement of children in a UIStack. */
enum class UIAlign : uint8_t { Start, Center, End, Stretch };

/** Axis-aligned rectangle; position is the top-left corner. */
struct UIRect {
	Vector2 position{0.0f};
	Vector2 size{0.0f};

	Vector2 max() const { return position + size; }
	bool contains(const Vector2& point) const {
		return point.x >= position.x && point.y >= position.y &&
			   point.x <= position.x + size.x &&
			   point.y <= position.y + size.y;
	}
	UIRect intersect(const UIRect& other) const {
		const Vector2 low = glm::max(position, other.position);
		const Vector2 high = glm::min(max(), other.max());
		return {low, glm::max(high - low, Vector2(0.0f))};
	}
	static UIRect unbounded() {
		constexpr float big = 1e9f;
		return {Vector2(-big), Vector2(2 * big)};
	}
};

}  // namespace bird

#endif	// !BIRD_UILAYOUT_H
