#include "UITabView.h"

#include <stdexcept>

#include "UIDrawList.h"

namespace bird {

void UITabButton::setSelected(bool selected) {
	if (selected != m_selected) {
		m_selected = selected;
		markVisualDirty();
	}
}

Vector4 UITabButton::backgroundColor() const {
	return m_selected ? theme().panelColor : UIButton::backgroundColor();
}

void UITabButton::draw(UIDrawList& list) {
	UIButton::draw(list);
	if (m_selected) {
		// Accent underline marks the active tab.
		list.rect({Vector2(0.0f, getSize().y - 2.0f), Vector2(getSize().x, 2.0f)},
				  theme().accentColor);
	}
}

UITabView::UITabView() : UIStack(UIAxis::Vertical) {
	setGap(0.0f);
	setAlign(UIAlign::Stretch);
	m_header = add<UIStack>(UIAxis::Horizontal);
	m_header->setGap(2.0f);
	m_pageArea = add<UIObject>();
	m_pageArea->setHeight(UISizeRule::fill());
	m_pageArea->setPadding(UIInsets::all(theme().padding));
}

UIObject* UITabView::addTab(std::string title, std::unique_ptr<UIObject> page) {
	const size_t index = m_pages.size();
	auto* button = m_header->add<UITabButton>(std::move(title));
	button->setOnClick([this, index] { selectTab(index); });
	page->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	UIObject* raw = m_pageArea->addChild(std::move(page));
	m_buttons.push_back(button);
	m_pages.push_back(raw);
	raw->setVisible(index == m_selected);
	button->setSelected(index == m_selected);
	return raw;
}

void UITabView::selectTab(size_t index) {
	if (index >= m_pages.size()) {
		throw std::out_of_range("UITabView::selectTab: no such tab");
	}
	if (index == m_selected && m_pages[index]->isVisible()) return;
	m_selected = index;
	for (size_t i = 0; i < m_pages.size(); i++) {
		m_pages[i]->setVisible(i == index);
		m_buttons[i]->setSelected(i == index);
	}
	if (m_onTabChanged) m_onTabChanged(index);
}

UITabButton* UITabView::getTabButton(size_t index) const {
	return m_buttons.at(index);
}

UITabView& UITabView::setOnTabChanged(
	std::function<void(size_t)> onTabChanged) {
	m_onTabChanged = std::move(onTabChanged);
	return *this;
}

}  // namespace bird
