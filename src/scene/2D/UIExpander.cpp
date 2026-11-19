#include "UIExpander.h"

#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kTriangleColumn = 20.0f;
}

UIExpander::UIExpander(std::string title, bool expanded)
	: UIStack(UIAxis::Vertical), m_title(std::move(title)), m_expanded(expanded) {
	setGap(0.0f);
	setAlign(UIAlign::Stretch);
	m_header = add<UIExpanderHeader>(*this);
	m_content = add<UIObject>();
	m_content->setPadding(UIInsets{kTriangleColumn, 6.0f, 0.0f, 6.0f});
	m_content->setVisible(expanded);
}

UIExpander& UIExpander::setExpanded(bool expanded) {
	if (expanded != m_expanded) {
		m_expanded = expanded;
		m_content->setVisible(expanded);
		markVisualDirty();
		if (m_onToggled) m_onToggled(expanded);
	}
	return *this;
}

UIExpander& UIExpander::setOnToggled(std::function<void(bool)> onToggled) {
	m_onToggled = std::move(onToggled);
	return *this;
}

UIExpanderHeader::UIExpanderHeader(UIExpander& owner) : m_owner(owner) {
	m_interactive = true;
	UISizeRule height = UISizeRule::fit();
	height.min = theme().rowHeight;
	setHeight(height);
}

bool UIExpanderHeader::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UIExpanderHeader::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		m_owner.toggle();
	}
	return true;
}

bool UIExpanderHeader::onKey(const UIKeyEvent& event) {
	if (event.key == UIKey::Space || event.key == UIKey::Enter) {
		m_owner.toggle();
		return true;
	}
	if ((event.key == UIKey::Right && !m_owner.isExpanded()) ||
		(event.key == UIKey::Left && m_owner.isExpanded())) {
		m_owner.toggle();
		return true;
	}
	return false;
}

Vector2 UIExpanderHeader::measureContent(Vector2 /*available*/) {
	const Vector2 text =
		ui::measureText(font(), m_owner.getTitle(), theme().titleFontSize);
	return Vector2(kTriangleColumn + text.x, text.y);
}

void UIExpanderHeader::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	if (isHovered() || isFocused()) {
		list.roundedRect(bounds, t.hoverColor, t.controlRadius,
						 isFocused() ? t.borderWidth : 0.0f, t.focusBorderColor);
	}
	const Vector2 c(kTriangleColumn * 0.5f, getSize().y * 0.5f);
	if (m_owner.isExpanded()) {
		list.triangle(c + Vector2(-4.0f, -2.5f), c + Vector2(0.0f, 3.0f),
					  c + Vector2(4.0f, -2.5f), t.textColor);
	} else {
		list.triangle(c + Vector2(-2.5f, -4.0f), c + Vector2(-2.5f, 4.0f),
					  c + Vector2(3.0f, 0.0f), t.textColor);
	}
	ui::drawTextCenteredY(list, font(), m_owner.getTitle(), t.titleFontSize,
						  kTriangleColumn, bounds, t.textColor);
	list.line(Vector2(0.0f, getSize().y - 0.5f),
			  Vector2(getSize().x, getSize().y - 0.5f), 1.0f, t.separatorColor);
}

}  // namespace bird
