#include "UIMenuBar.h"

#include "UIManager.h"
#include "UIMenuPanel.h"
#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kTitlePadding = 10.0f;
}

UIMenuBar::UIMenuBar() {
	m_interactive = true;
	setPadding(UIInsets::symmetric(4.0f, 2.0f));
}

UIMenuBar& UIMenuBar::addMenu(std::string title, std::vector<UIMenuItem> items) {
	m_menus.push_back({std::move(title), makeMenu(std::move(items))});
	markLayoutDirty();
	return *this;
}

float UIMenuBar::titleWidth(size_t index) const {
	return ui::measureText(font(), m_menus[index].title, theme().fontSize).x +
		   2.0f * kTitlePadding;
}

UIRect UIMenuBar::titleRect(size_t index) const {
	float x = getPadding().left;
	for (size_t i = 0; i < index; i++) x += titleWidth(i);
	return {Vector2(x, getPadding().top),
			Vector2(titleWidth(index), contentSize().y)};
}

size_t UIMenuBar::titleAt(float localX) const {
	float x = getPadding().left;
	for (size_t i = 0; i < m_menus.size(); i++) {
		const float w = titleWidth(i);
		if (localX >= x && localX < x + w) return i;
		x += w;
	}
	return kNone;
}

Vector2 UIMenuBar::measureContent(Vector2 /*available*/) {
	float width = 0.0f;
	for (size_t i = 0; i < m_menus.size(); i++) width += titleWidth(i);
	return Vector2(width, theme().controlHeight - 4.0f);
}

void UIMenuBar::openMenu(size_t index, bool fromKeyboard) {
	UIManager* manager = getManager();
	if (!manager || index >= m_menus.size()) return;
	manager->closeAllPopups();
	manager->layoutNow();
	const UIRect title = titleRect(index);
	const Vector3 origin = getWorldMatrix() * Vector3(title.position, 1.0f);
	auto panel = std::make_unique<UIMenuPanel>(m_menus[index].items, nullptr,
											   this, false);
	m_panel = panel.get();
	m_open = index;
	manager->openPopup(this, std::move(panel),
					   UIRect{Vector2(origin), title.size});
	if (fromKeyboard) m_panel->highlightFirst();
	markVisualDirty();
}

void UIMenuBar::switchMenu(int delta) {
	if (m_menus.empty()) return;
	const auto count = static_cast<long>(m_menus.size());
	const long current = m_open == kNone ? 0 : static_cast<long>(m_open);
	openMenu(static_cast<size_t>(((current + delta) % count + count) % count),
			 true);
}

void UIMenuBar::onPopupClosed(UIObject* popup) {
	if (popup == m_panel) {
		m_panel = nullptr;
		m_open = kNone;
		markVisualDirty();
	}
}

bool UIMenuBar::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	// A press while a menu is open is consumed by light dismiss (closing
	// it), so reaching here means no menu is open: open the one clicked.
	const size_t index = titleAt(toLocal(event.position).x);
	if (index != kNone) openMenu(index);
	return true;
}

bool UIMenuBar::onMouseMove(const UIMouseEvent& event) {
	const size_t index = titleAt(toLocal(event.position).x);
	if (index != m_hoveredTitle) {
		m_hoveredTitle = index;
		markVisualDirty();
	}
	// With a menu open, hovering another title switches to its menu.
	if (m_open != kNone && index != kNone && index != m_open) openMenu(index);
	return true;
}

void UIMenuBar::onHover(bool hovered) {
	if (!hovered && m_hoveredTitle != kNone) {
		m_hoveredTitle = kNone;
		markVisualDirty();
	}
}

void UIMenuBar::draw(UIDrawList& list) {
	const UITheme& t = theme();
	list.rect({Vector2(0.0f), getSize()}, t.titleBarColor);
	list.rect({Vector2(0.0f, getSize().y - 1.0f), Vector2(getSize().x, 1.0f)},
			  t.separatorColor);
	for (size_t i = 0; i < m_menus.size(); i++) {
		const UIRect title = titleRect(i);
		if (i == m_open) {
			list.roundedRect(title, t.menuHighlightColor, t.controlRadius);
		} else if (i == m_hoveredTitle) {
			list.roundedRect(title, t.hoverColor, t.controlRadius);
		}
		ui::drawTextCentered(list, font(), m_menus[i].title, t.fontSize, title,
							 t.textColor);
	}
}

}  // namespace bird
