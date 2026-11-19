#include "UICheckBox.h"

#include <algorithm>

#include "UITextUtil.h"

namespace bird {

UICheckBox::UICheckBox(std::string text, bool checked)
	: m_text(std::move(text)), m_checked(checked) {
	m_interactive = true;
}

UICheckBox& UICheckBox::setChecked(bool checked) {
	if (checked != m_checked) {
		m_checked = checked;
		markVisualDirty();
		if (m_onChanged) m_onChanged(m_checked);
	}
	return *this;
}

UICheckBox& UICheckBox::setText(std::string text) {
	m_text = std::move(text);
	markLayoutDirty();
	return *this;
}

UICheckBox& UICheckBox::setOnChanged(std::function<void(bool)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UICheckBox::toggle() {
	if (isEffectivelyEnabled()) setChecked(!m_checked);
}

bool UICheckBox::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UICheckBox::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		toggle();
	}
	return true;
}

bool UICheckBox::onKey(const UIKeyEvent& event) {
	if (event.key == UIKey::Space || event.key == UIKey::Enter) {
		toggle();
		return true;
	}
	return false;
}

Vector2 UICheckBox::measureContent(Vector2 /*available*/) {
	const float box = theme().checkBoxSize;
	const Vector2 text = ui::measureText(font(), m_text, theme().fontSize);
	const float gap = m_text.empty() ? 0.0f : theme().itemSpacing;
	return Vector2(box + gap + text.x, std::max(box, text.y));
}

void UICheckBox::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const float box = t.checkBoxSize;
	const Vector2 origin = getPadding().start();
	const UIRect content{origin, contentSize()};
	const UIRect boxRect{origin + Vector2(0.0f, (content.size.y - box) * 0.5f),
						 Vector2(box)};
	const Vector4 edge = isFocused() ? t.focusBorderColor : t.borderColor;
	if (m_checked) {
		list.roundedRect(boxRect,
						 isEffectivelyEnabled() ? t.accentColor : t.disabledText,
						 t.controlRadius, isFocused() ? t.borderWidth : 0.0f,
						 edge);
		// Check mark: two strokes in box-relative coordinates.
		const Vector2 o = boxRect.position;
		const Vector2 a = o + Vector2(0.24f, 0.52f) * box;
		const Vector2 b = o + Vector2(0.43f, 0.70f) * box;
		const Vector2 c = o + Vector2(0.77f, 0.30f) * box;
		list.line(a, b, 2.0f, t.textColor);
		list.line(b, c, 2.0f, t.textColor);
	} else {
		list.roundedRect(boxRect, isHovered() ? t.hoverColor : t.inputColor,
						 t.controlRadius, t.borderWidth, edge);
	}
	ui::drawTextCenteredY(list, font(), m_text, t.fontSize,
						  origin.x + box + t.itemSpacing, content,
						  isEffectivelyEnabled() ? t.textColor : t.disabledText);
}

}  // namespace bird
