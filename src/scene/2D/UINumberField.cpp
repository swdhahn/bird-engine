#include "UINumberField.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "UIDrawList.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 90.0f;
}

UINumberField::UINumberField(double min, double max, double value)
	: m_min(min), m_max(max), m_value(0.0) {
	if (max <= min) throw std::invalid_argument("UINumberField: max <= min");
	UIInsets padding = getPadding();
	padding.right += kStepperWidth;	 // steppers live in the right padding
	setPadding(padding);
	// Clamp only: snapping waits for setStep, so the default step cannot
	// shift the requested value (e.g. 1.0 from a 0.1 minimum).
	m_value = std::clamp(value, min, max);
	refreshText();
}

UINumberField& UINumberField::setRange(double min, double max) {
	if (max <= min) throw std::invalid_argument("UINumberField: max <= min");
	m_min = min;
	m_max = max;
	return setValue(m_value);
}

UINumberField& UINumberField::setStep(double step) {
	m_step = std::max(step, 0.0);
	return setValue(m_value);
}

UINumberField& UINumberField::setPrecision(int digits) {
	m_precision = std::clamp(digits, 0, 10);
	refreshText();
	return *this;
}

double UINumberField::snap(double value) const {
	value = std::clamp(value, m_min, m_max);
	if (m_step > 0.0) {
		value = m_min + std::round((value - m_min) / m_step) * m_step;
		value = std::clamp(value, m_min, m_max);
	}
	return value;
}

void UINumberField::refreshText() {
	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%.*f", m_precision, m_value);
	m_state.setText(buffer);
	afterEdit(false);
}

UINumberField& UINumberField::setValue(double value) {
	const double snapped = snap(value);
	const bool changed = snapped != m_value;
	m_value = snapped;
	refreshText();
	if (changed && m_onValueChanged) m_onValueChanged(m_value);
	return *this;
}

UINumberField& UINumberField::setOnValueChanged(
	std::function<void(double)> onChanged) {
	m_onValueChanged = std::move(onChanged);
	return *this;
}

void UINumberField::commit() {
	const std::string& text = m_state.text();
	char* end = nullptr;
	const double parsed = std::strtod(text.c_str(), &end);
	const bool valid = !text.empty() && end == text.c_str() + text.size() &&
					   std::isfinite(parsed);
	if (valid) {
		setValue(parsed);
	} else {
		refreshText();	// revert to the last valid value
	}
}

double UINumberField::effectiveStep() const {
	return m_step > 0.0 ? m_step : (m_max - m_min) / 100.0;
}

void UINumberField::stepBy(double steps) {
	setValue(m_value + steps * effectiveStep());
}

bool UINumberField::acceptsCharacter(uint32_t c) const {
	return (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
		   c == 'e' || c == 'E';
}

bool UINumberField::inSteppers(Vector2 local) const {
	return local.x >= getSize().x - kStepperWidth && local.x <= getSize().x &&
		   local.y >= 0.0f && local.y <= getSize().y;
}

bool UINumberField::onMouseDown(const UIMouseEvent& event) {
	const Vector2 local = toLocal(event.position);
	if (event.button == UIMouseButton::Left && inSteppers(local)) {
		commit();
		stepBy(local.y < getSize().y * 0.5f ? 1.0 : -1.0);
		m_scrubbing = true;
		m_scrubStartY = event.position.y;
		m_scrubStartValue = m_value;
		return true;
	}
	return UITextBox::onMouseDown(event);
}

bool UINumberField::onMouseMove(const UIMouseEvent& event) {
	m_lastLocalMouse = toLocal(event.position);
	if (m_scrubbing && isPressed()) {
		// Dragging up increases the value, one step per kScrubPixels.
		const double steps =
			std::floor((m_scrubStartY - event.position.y) / kScrubPixels);
		setValue(m_scrubStartValue + steps * effectiveStep());
		return true;
	}
	return UITextBox::onMouseMove(event);
}

bool UINumberField::onMouseUp(const UIMouseEvent& event) {
	m_scrubbing = false;
	return UITextBox::onMouseUp(event);
}

bool UINumberField::onScroll(const UIScrollEvent& event) {
	if (!isFocused() && !isHovered()) return false;
	commit();
	stepBy(event.delta.y > 0.0f ? 1.0 : -1.0);
	return true;
}

bool UINumberField::onKey(const UIKeyEvent& event) {
	const double multiplier = event.shift() ? 10.0 : 1.0;
	switch (event.key) {
		case UIKey::Up:
			commit();
			stepBy(multiplier);
			return true;
		case UIKey::Down:
			commit();
			stepBy(-multiplier);
			return true;
		case UIKey::Enter:
			commit();
			return UITextBox::onKey(event);	 // still fires onSubmit
		default:
			return UITextBox::onKey(event);
	}
}

void UINumberField::onFocus(bool focused) {
	if (!focused) commit();
	UITextBox::onFocus(focused);
}

UICursor UINumberField::cursor() const {
	return inSteppers(m_lastLocalMouse) || m_scrubbing
			   ? UICursor::ResizeVertical
			   : UICursor::IBeam;
}

Vector2 UINumberField::measureContent(Vector2 available) {
	return Vector2(kDefaultWidth, UITextBox::measureContent(available).y);
}

void UINumberField::draw(UIDrawList& list) {
	UITextBox::draw(list);
	const UITheme& t = theme();
	const float x = getSize().x - kStepperWidth;
	const float midY = getSize().y * 0.5f;
	list.line(Vector2(x, 4.0f), Vector2(x, getSize().y - 4.0f), 1.0f,
			  t.separatorColor);
	const float cx = x + kStepperWidth * 0.5f;
	const float w = 4.0f, h = 3.5f;
	const Vector4 arrow = isEffectivelyEnabled() ? t.mutedTextColor : t.disabledText;
	list.triangle(Vector2(cx - w, midY - 3.0f), Vector2(cx + w, midY - 3.0f),
				  Vector2(cx, midY - 3.0f - h * 1.6f), arrow);
	list.triangle(Vector2(cx - w, midY + 3.0f), Vector2(cx, midY + 3.0f + h * 1.6f),
				  Vector2(cx + w, midY + 3.0f), arrow);
}

}  // namespace bird
