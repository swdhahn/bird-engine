#include "UIColorControls.h"

#include <algorithm>
#include <cmath>

#include "UIColor.h"
#include "UIDrawList.h"

namespace bird {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kBarThickness = 18.0f;
constexpr float kBarLength = 160.0f;

Vector4 opaque(const Vector3& rgb) {
	return Vector4(rgb, 1.0f);
}
}  // namespace

void ui::drawTransparencyChecker(UIDrawList& list, const UIRect& rect) {
	list.checkerboard(rect, 6.0f, Vector4(0.80f, 0.80f, 0.80f, 1.0f),
					  Vector4(0.55f, 0.55f, 0.55f, 1.0f));
}

// ---------------------------------------------------------- UIColorWheel

UIColorWheel::UIColorWheel(float diameter) {
	m_interactive = true;
	setSize(Vector2(diameter));
}

float UIColorWheel::radius() const {
	return std::max(std::min(getSize().x, getSize().y) * 0.5f - 2.0f, 1.0f);
}

void UIColorWheel::setHueSaturation(float hue, float saturation) {
	m_hue = std::fmod(std::fmod(hue, 360.0f) + 360.0f, 360.0f);
	m_saturation = std::clamp(saturation, 0.0f, 1.0f);
	markVisualDirty();
}

UIColorWheel& UIColorWheel::setOnChanged(
	std::function<void(float, float)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

std::pair<float, float> UIColorWheel::hueSaturationAt(Vector2 local) const {
	const Vector2 d = local - center();
	// Screen y grows downwards; negate it so hue turns counter-clockwise.
	float hue = std::atan2(-d.y, d.x) * 180.0f / kPi;
	if (hue < 0.0f) hue += 360.0f;
	return {hue, std::min(glm::length(d) / radius(), 1.0f)};
}

Vector2 UIColorWheel::pointFor(float hue, float saturation) const {
	const float a = hue * kPi / 180.0f;
	return center() + Vector2(std::cos(a), -std::sin(a)) * (saturation * radius());
}

void UIColorWheel::change(float hue, float saturation) {
	setHueSaturation(hue, saturation);
	if (m_onChanged) m_onChanged(m_hue, m_saturation);
}

void UIColorWheel::pick(Vector2 local) {
	const auto [hue, saturation] = hueSaturationAt(local);
	// At the exact centre hue is undefined: keep the current one.
	change(saturation > 0.0f ? hue : m_hue, saturation);
}

bool UIColorWheel::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	pick(toLocal(event.position));
	return true;
}

bool UIColorWheel::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	pick(toLocal(event.position));
	return true;
}

bool UIColorWheel::onKey(const UIKeyEvent& event) {
	const float hueStep = event.shift() ? 15.0f : 2.0f;
	const float satStep = event.shift() ? 0.1f : 0.02f;
	switch (event.key) {
		case UIKey::Left:
			change(m_hue - hueStep, m_saturation);
			return true;
		case UIKey::Right:
			change(m_hue + hueStep, m_saturation);
			return true;
		case UIKey::Up:
			change(m_hue, m_saturation + satStep);
			return true;
		case UIKey::Down:
			change(m_hue, m_saturation - satStep);
			return true;
		default:
			return false;
	}
}

void UIColorWheel::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const Vector2 c = center();
	const float r = radius();
	// Triangle fan: white centre to fully saturated rim. RGB interpolation
	// from white to a hue is exactly the saturation ramp at full value.
	const Vector4 white(1.0f);
	for (int i = 0; i < kSegments; i++) {
		const float h0 = 360.0f * static_cast<float>(i) / kSegments;
		const float h1 = 360.0f * static_cast<float>(i + 1) / kSegments;
		list.triangle(c, pointFor(h0, 1.0f), pointFor(h1, 1.0f), white,
					  opaque(color::hsvToRgb({h0, 1.0f, 1.0f})),
					  opaque(color::hsvToRgb({h1, 1.0f, 1.0f})));
	}
	// Anti-aliased rim over the fan's straight edges.
	list.roundedRect({c - Vector2(r + 1.0f), Vector2(2.0f * r + 2.0f)}, Vector4(0.0f),
					 r + 1.0f, 2.0f, isFocused() ? t.focusBorderColor : t.borderColor);
	// Marker: dark outer ring and white inner ring, readable on any hue.
	const Vector2 p = pointFor(m_hue, m_saturation);
	list.roundedRect({p - Vector2(7.0f), Vector2(14.0f)}, Vector4(0.0f), 7.0f, 2.0f,
					 Vector4(0.0f, 0.0f, 0.0f, 0.7f));
	list.roundedRect({p - Vector2(5.0f), Vector2(10.0f)},
					 opaque(color::hsvToRgb({m_hue, m_saturation, 1.0f})), 5.0f, 2.0f,
					 Vector4(1.0f));
}

// --------------------------------------------------------- UIColorSlider

UIColorSlider::UIColorSlider(UIAxis axis) : m_axis(axis) {
	m_interactive = true;
}

Vector2 UIColorSlider::measureContent(Vector2 /*available*/) {
	return m_axis == UIAxis::Horizontal ? Vector2(kBarLength, kBarThickness)
										: Vector2(kBarThickness, kBarLength);
}

void UIColorSlider::setValue(float value) {
	m_value = std::clamp(value, 0.0f, 1.0f);
	markVisualDirty();
}

UIColorSlider& UIColorSlider::setOnChanged(std::function<void(float)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UIColorSlider::setGradient(const Vector4& atZero, const Vector4& atOne) {
	m_atZero = atZero;
	m_atOne = atOne;
	markVisualDirty();
}

UIColorSlider& UIColorSlider::setCheckered(bool checkered) {
	m_checkered = checkered;
	markVisualDirty();
	return *this;
}

float UIColorSlider::valueAt(Vector2 local) const {
	const Vector2 size = glm::max(getSize(), Vector2(1.0f));
	const float v = m_axis == UIAxis::Horizontal ? local.x / size.x
												 : 1.0f - local.y / size.y;
	return std::clamp(v, 0.0f, 1.0f);
}

void UIColorSlider::change(float value) {
	setValue(value);
	if (m_onChanged) m_onChanged(m_value);
}

bool UIColorSlider::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	change(valueAt(toLocal(event.position)));
	return true;
}

bool UIColorSlider::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	change(valueAt(toLocal(event.position)));
	return true;
}

bool UIColorSlider::onKey(const UIKeyEvent& event) {
	const float step = event.shift() ? 0.1f : 0.01f;
	switch (event.key) {
		case UIKey::Left:
		case UIKey::Down:
			change(m_value - step);
			return true;
		case UIKey::Right:
		case UIKey::Up:
			change(m_value + step);
			return true;
		case UIKey::Home:
			change(0.0f);
			return true;
		case UIKey::End:
			change(1.0f);
			return true;
		default:
			return false;
	}
}

void UIColorSlider::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	if (m_checkered) ui::drawTransparencyChecker(list, bounds);
	if (m_axis == UIAxis::Horizontal) {
		list.gradientRect(bounds, m_atZero, m_atOne, m_atZero, m_atOne);
	} else {
		list.gradientRect(bounds, m_atOne, m_atOne, m_atZero, m_atZero);
	}
	list.roundedRect(bounds, Vector4(0.0f), 3.0f, isFocused() ? 2.0f : 1.0f,
					 isFocused() ? t.focusBorderColor : t.borderColor);
	// Handle: a white bar with a dark outline across the track.
	const float handle = 5.0f;
	UIRect h;
	if (m_axis == UIAxis::Horizontal) {
		const float x = m_value * bounds.size.x;
		h = {Vector2(x - handle * 0.5f, -2.0f), Vector2(handle, bounds.size.y + 4.0f)};
	} else {
		const float y = (1.0f - m_value) * bounds.size.y;
		h = {Vector2(-2.0f, y - handle * 0.5f), Vector2(bounds.size.x + 4.0f, handle)};
	}
	list.roundedRect(h, Vector4(1.0f), 2.0f, 1.0f, Vector4(0.0f, 0.0f, 0.0f, 0.75f));
}

// --------------------------------------------------------- UIColorSwatch

UIColorSwatch::UIColorSwatch() {
	m_interactive = true;
}

Vector2 UIColorSwatch::measureContent(Vector2 /*available*/) {
	return Vector2(120.0f, 40.0f);
}

void UIColorSwatch::setColor(const Vector4& color) {
	m_color = color;
	markVisualDirty();
}

void UIColorSwatch::setOriginal(const Vector4& original) {
	m_original = original;
	m_hasOriginal = true;
	markVisualDirty();
}

UIColorSwatch& UIColorSwatch::setOnRevert(
	std::function<void(const Vector4&)> onRevert) {
	m_onRevert = std::move(onRevert);
	return *this;
}

bool UIColorSwatch::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	if (m_hasOriginal && toLocal(event.position).x < getSize().x * 0.5f &&
		m_onRevert) {
		m_onRevert(m_original);
	}
	return true;
}

void UIColorSwatch::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	ui::drawTransparencyChecker(list, bounds);
	if (m_hasOriginal) {
		const Vector2 half(bounds.size.x * 0.5f, bounds.size.y);
		list.rect({Vector2(0.0f), half}, m_original);
		list.rect({Vector2(half.x, 0.0f), half}, m_color);
	} else {
		list.rect(bounds, m_color);
	}
	list.roundedRect(bounds, Vector4(0.0f), 4.0f, 1.0f, t.borderColor);
}

}  // namespace bird
