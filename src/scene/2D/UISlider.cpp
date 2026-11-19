#include "UISlider.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "UIDrawList.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 160.0f;
constexpr float kTrackHeight = 4.0f;
constexpr float kThumbHeight = 16.0f;
constexpr float kKeyboardFraction = 0.05f;	// step when snapping is off
}  // namespace

UISlider::UISlider(float min, float max, float value) : m_min(min), m_max(max) {
	if (max <= min) throw std::invalid_argument("UISlider: max <= min");
	m_interactive = true;
	m_value = std::clamp(value, min, max);
}

UISlider& UISlider::setRange(float min, float max) {
	if (max <= min) throw std::invalid_argument("UISlider: max <= min");
	m_min = min;
	m_max = max;
	return setValue(m_value);
}

UISlider& UISlider::setStep(float step) {
	m_step = std::max(step, 0.0f);
	return setValue(m_value);
}

float UISlider::snap(float value) const {
	value = std::clamp(value, m_min, m_max);
	if (m_step > 0.0f) {
		value = m_min + std::round((value - m_min) / m_step) * m_step;
		value = std::clamp(value, m_min, m_max);
	}
	return value;
}

UISlider& UISlider::setValue(float value) {
	const float snapped = snap(value);
	if (snapped != m_value) {
		m_value = snapped;
		markVisualDirty();
		if (m_onChanged) m_onChanged(m_value);
	}
	return *this;
}

UISlider& UISlider::setOnChanged(std::function<void(float)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

float UISlider::getFraction() const {
	return (m_value - m_min) / (m_max - m_min);
}

float UISlider::trackStart() const {
	return getPadding().left + theme().sliderThumbWidth * 0.5f;
}

float UISlider::trackLength() const {
	return std::max(contentSize().x - theme().sliderThumbWidth, 1.0f);
}

float UISlider::valueAt(float localX) const {
	const float fraction =
		std::clamp((localX - trackStart()) / trackLength(), 0.0f, 1.0f);
	return m_min + fraction * (m_max - m_min);
}

bool UISlider::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	setValue(valueAt(toLocal(event.position).x));
	return true;
}

bool UISlider::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	setValue(valueAt(toLocal(event.position).x));
	return true;
}

bool UISlider::onMouseUp(const UIMouseEvent& /*event*/) {
	return true;
}

bool UISlider::onKey(const UIKeyEvent& event) {
	const float step =
		m_step > 0.0f ? m_step : (m_max - m_min) * kKeyboardFraction;
	switch (event.key) {
		case UIKey::Left:
		case UIKey::Down:
			setValue(m_value - step);
			return true;
		case UIKey::Right:
		case UIKey::Up:
			setValue(m_value + step);
			return true;
		case UIKey::Home:
			setValue(m_min);
			return true;
		case UIKey::End:
			setValue(m_max);
			return true;
		default:
			return false;
	}
}

Vector2 UISlider::measureContent(Vector2 /*available*/) {
	return Vector2(kDefaultWidth, theme().controlHeight);
}

void UISlider::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const float centerY = getPadding().top + contentSize().y * 0.5f;
	const float start = trackStart();
	const float thumbX = start + getFraction() * trackLength();
	list.rect({Vector2(start, centerY - kTrackHeight * 0.5f),
			   Vector2(trackLength(), kTrackHeight)},
			  t.inputColor);
	list.rect({Vector2(start, centerY - kTrackHeight * 0.5f),
			   Vector2(thumbX - start, kTrackHeight)},
			  isEffectivelyEnabled() ? t.accentColor : t.disabledText);
	const UIRect thumb{Vector2(thumbX - t.sliderThumbWidth * 0.5f,
							   centerY - kThumbHeight * 0.5f),
					   Vector2(t.sliderThumbWidth, kThumbHeight)};
	list.rect(thumb, isPressed() || isHovered() ? t.textColor : t.mutedTextColor);
	if (isFocused()) list.border(thumb, t.borderWidth, t.focusBorderColor);
}

}  // namespace bird
