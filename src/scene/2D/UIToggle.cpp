#include "UIToggle.h"

#include <algorithm>
#include <cmath>

#include "UIManager.h"
#include "UITextUtil.h"

namespace bird {

UIToggle::UIToggle(std::string text, bool on)
	: m_text(std::move(text)), m_on(on), m_knob(on ? 1.0f : 0.0f) {
	m_interactive = true;
}

UIToggle& UIToggle::setOn(bool on) {
	if (on == m_on) return *this;
	m_on = on;
	const float target = on ? 1.0f : 0.0f;
	if (UIManager* manager = getManager()) {
		manager->getAnimationSystem().cancel(m_knobAnimation);
	}
	// Constant knob speed: a reversal mid-slide takes proportionally less
	// time. Detached toggles snap (UIAnimator::start handles that).
	m_knobAnimation =
		animate()
			.value(m_knob, target,
				   [this](float knob) {
					   m_knob = knob;
					   markVisualDirty();
				   })
			.duration(theme().animationSeconds * std::fabs(target - m_knob))
			.ease(Ease::Linear)
			.start();
	markVisualDirty();
	if (m_onChanged) m_onChanged(m_on);
	return *this;
}

UIToggle& UIToggle::setText(std::string text) {
	m_text = std::move(text);
	markLayoutDirty();
	return *this;
}

UIToggle& UIToggle::setOnChanged(std::function<void(bool)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UIToggle::toggle() {
	if (isEffectivelyEnabled()) setOn(!m_on);
}

bool UIToggle::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UIToggle::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		toggle();
	}
	return true;
}

bool UIToggle::onKey(const UIKeyEvent& event) {
	if (event.key == UIKey::Space || event.key == UIKey::Enter) {
		toggle();
		return true;
	}
	return false;
}

Vector2 UIToggle::measureContent(Vector2 /*available*/) {
	const UITheme& t = theme();
	const Vector2 text = ui::measureText(font(), m_text, t.fontSize);
	const float gap = m_text.empty() ? 0.0f : t.itemSpacing;
	return Vector2(t.switchWidth + gap + text.x,
				   std::max(t.switchHeight, text.y));
}

void UIToggle::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const Vector2 origin = getPadding().start();
	const UIRect content{origin, contentSize()};
	const UIRect track{
		origin + Vector2(0.0f, (content.size.y - t.switchHeight) * 0.5f),
		Vector2(t.switchWidth, t.switchHeight)};
	const float radius = t.switchHeight * 0.5f;
	const Vector4 on = isEffectivelyEnabled() ? t.accentColor : t.disabledText;
	list.roundedRect(track, glm::mix(t.switchOffColor, on, m_knob), radius,
					 isFocused() ? 2.0f : 0.0f, t.focusBorderColor);
	const float inset = 3.0f;
	const float knobSize = t.switchHeight - 2.0f * inset;
	const float travel = t.switchWidth - 2.0f * inset - knobSize;
	list.roundedRect({track.position + Vector2(inset + travel * m_knob, inset),
					  Vector2(knobSize)},
					 t.textColor, knobSize * 0.5f);
	ui::drawTextCenteredY(list, font(), m_text, t.fontSize,
						  origin.x + t.switchWidth + t.itemSpacing, content,
						  isEffectivelyEnabled() ? t.textColor : t.disabledText);
}

}  // namespace bird
