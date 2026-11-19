#include "UIRadioGroup.h"

#include <algorithm>
#include <stdexcept>

#include "UIManager.h"
#include "UITextUtil.h"

namespace bird {

// --------------------------------------------------------- UIRadioButton

UIRadioButton::UIRadioButton(UIRadioGroup& group, size_t index,
							 std::string text)
	: m_group(group), m_index(index), m_text(std::move(text)) {
	m_interactive = true;
}

bool UIRadioButton::isSelected() const {
	return m_group.getSelected() == m_index;
}

bool UIRadioButton::isFocusable() const {
	const size_t selected = m_group.getSelected();
	return selected == UIRadioGroup::kNone ? m_index == 0 : isSelected();
}

bool UIRadioButton::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UIRadioButton::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position) &&
		isEffectivelyEnabled()) {
		m_group.setSelected(m_index);
		// Now the group's tab stop, so it can take focus.
		if (UIManager* manager = getManager()) manager->setFocus(this);
	}
	return true;
}

bool UIRadioButton::onKey(const UIKeyEvent& event) {
	switch (event.key) {
		case UIKey::Up:
		case UIKey::Left:
			m_group.moveSelection(-1);
			return true;
		case UIKey::Down:
		case UIKey::Right:
			m_group.moveSelection(+1);
			return true;
		case UIKey::Space:
		case UIKey::Enter:
			m_group.setSelected(m_index);
			return true;
		default:
			return false;
	}
}

Vector2 UIRadioButton::measureContent(Vector2 /*available*/) {
	const float box = theme().checkBoxSize;
	const Vector2 text = ui::measureText(font(), m_text, theme().fontSize);
	return Vector2(box + theme().itemSpacing + text.x, std::max(box, text.y));
}

void UIRadioButton::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const float box = t.checkBoxSize;
	const Vector2 origin = getPadding().start();
	const UIRect content{origin, contentSize()};
	const UIRect circle{origin + Vector2(0.0f, (content.size.y - box) * 0.5f),
						Vector2(box)};
	list.roundedRect(circle, isHovered() ? t.hoverColor : t.inputColor,
					 box * 0.5f, t.borderWidth,
					 isFocused() ? t.focusBorderColor : t.borderColor);
	if (isSelected()) {
		const float dot = box * 0.5f;
		list.roundedRect({circle.position + Vector2((box - dot) * 0.5f),
						  Vector2(dot)},
						 isEffectivelyEnabled() ? t.accentColor : t.disabledText,
						 dot * 0.5f);
	}
	ui::drawTextCenteredY(list, font(), m_text, t.fontSize,
						  origin.x + box + t.itemSpacing, content,
						  isEffectivelyEnabled() ? t.textColor : t.disabledText);
}

// ---------------------------------------------------------- UIRadioGroup

UIRadioGroup::UIRadioGroup(UIAxis axis) : UIStack(axis) {}

UIRadioButton* UIRadioGroup::addOption(std::string text) {
	auto* option = add<UIRadioButton>(*this, m_options.size(), std::move(text));
	m_options.push_back(option);
	return option;
}

UIRadioGroup& UIRadioGroup::setSelected(size_t index) {
	if (index != kNone && index >= m_options.size()) {
		throw std::out_of_range("UIRadioGroup::setSelected: no such option");
	}
	if (index != m_selected) {
		m_selected = index;
		markVisualDirty();
		if (m_onChanged) m_onChanged(m_selected);
	}
	return *this;
}

UIRadioGroup& UIRadioGroup::setOnChanged(
	std::function<void(size_t)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UIRadioGroup::moveSelection(int delta) {
	if (m_options.empty()) return;
	const auto count = static_cast<long>(m_options.size());
	const long current = m_selected == kNone ? 0 : static_cast<long>(m_selected);
	const long next = ((current + delta) % count + count) % count;
	setSelected(static_cast<size_t>(next));
	if (UIManager* manager = getManager()) {
		manager->setFocus(m_options[static_cast<size_t>(next)]);
	}
}

}  // namespace bird
