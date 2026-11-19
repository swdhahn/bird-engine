#include "UIPanel.h"

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

void UIPanel::draw(UIDrawList& list) {
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.rect(bounds, m_background.value_or(theme().panelColor));
	if (m_border) list.border(bounds, theme().borderWidth, theme().borderColor);
}

}  // namespace bird
