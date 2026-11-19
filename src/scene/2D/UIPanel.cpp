#include "UIPanel.h"

#include <algorithm>

#include "UIDrawList.h"

namespace bird {

UIPanel::UIPanel() {
	m_interactive = true;
	setPadding(UIInsets::all(theme().padding));
}

UIPanel& UIPanel::setBackground(const Vector4& color) {
	m_background = color;
	markVisualDirty();
	return *this;
}

UIPanel& UIPanel::setBorder(bool border) {
	m_border = border;
	markVisualDirty();
	return *this;
}

UIPanel& UIPanel::setClipChildren(bool clip) {
	m_clipChildren = clip;
	markLayoutDirty();
	return *this;
}

UIPanel& UIPanel::setCornerRadius(float radius) {
	m_radius = std::max(radius, 0.0f);
	markVisualDirty();
	return *this;
}

UIPanel& UIPanel::setShadow(bool shadow) {
	m_shadow = shadow;
	markVisualDirty();
	return *this;
}

void UIPanel::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	const float radius = m_radius.value_or(t.panelRadius);
	if (m_shadow) list.shadow(bounds, radius, t.shadowBlur, t.shadowColor);
	list.roundedRect(bounds, m_background.value_or(t.panelColor), radius,
					 m_border ? t.borderWidth : 0.0f, t.borderColor);
}

}  // namespace bird
