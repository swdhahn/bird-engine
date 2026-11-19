#include "UIButton.h"

#include "UITextUtil.h"

namespace bird {

UIButton::UIButton(std::string text) : m_text(std::move(text)) {
	m_interactive = true;
	setPadding(UIInsets::symmetric(theme().padding * 1.5f, theme().padding * 0.5f));
	UISizeRule height = UISizeRule::fit();
	height.min = theme().controlHeight;
	setHeight(height);
}

UIButton& UIButton::setText(std::string text) {
	if (text != m_text) {
		m_text = std::move(text);
		markLayoutDirty();
	}
	return *this;
}

UIButton& UIButton::setOnClick(std::function<void()> onClick) {
	m_onClick = std::move(onClick);
	return *this;
}

void UIButton::click() {
	if (isEffectivelyEnabled() && m_onClick) m_onClick();
}

bool UIButton::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UIButton::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		click();
	}
	return true;
}

bool UIButton::onKey(const UIKeyEvent& event) {
	if (event.key == UIKey::Enter || event.key == UIKey::Space) {
		click();
		return true;
	}
	return false;
}

Vector2 UIButton::measureContent(Vector2 /*available*/) {
	return ui::measureText(font(), m_text, theme().fontSize);
}

Vector4 UIButton::backgroundColor() const {
	const UITheme& t = theme();
	if (!isEffectivelyEnabled()) return t.disabledColor;
	if (isPressed()) return t.activeColor;
	if (isHovered()) return t.hoverColor;
	return t.surfaceColor;
}

void UIButton::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.rect(bounds, backgroundColor());
	list.border(bounds, t.borderWidth,
				isFocused() ? t.focusBorderColor : t.borderColor);
	ui::drawTextCentered(list, font(), m_text, t.fontSize, bounds,
						 isEffectivelyEnabled() ? t.textColor : t.disabledText);
}

}  // namespace bird
