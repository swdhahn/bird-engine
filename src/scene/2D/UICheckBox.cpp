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
	list.rect(boxRect, isHovered() ? t.hoverColor : t.inputColor);
	list.border(boxRect, t.borderWidth,
				isFocused() ? t.focusBorderColor : t.borderColor);
	if (m_checked) {
		const float inset = box * 0.25f;
		list.rect({boxRect.position + Vector2(inset), Vector2(box - 2 * inset)},
				  isEffectivelyEnabled() ? t.accentColor : t.disabledText);
	}
	ui::drawTextCenteredY(list, font(), m_text, t.fontSize,
						  origin.x + box + t.itemSpacing, content,
						  isEffectivelyEnabled() ? t.textColor : t.disabledText);
}

}  // namespace bird
