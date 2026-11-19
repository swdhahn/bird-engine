#include "UIMenuPanel.h"

#include <algorithm>
#include <cctype>

#include "UIManager.h"
#include "UIMenuBar.h"
#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kPanelPadding = 4.0f;
constexpr float kCheckColumn = 22.0f;
constexpr float kShortcutGap = 28.0f;
constexpr float kArrowColumn = 16.0f;
constexpr float kSeparatorHeight = 9.0f;
constexpr float kMinWidth = 140.0f;
}  // namespace

UIMenuPanel::UIMenuPanel(UIMenuItems root, std::vector<UIMenuItem>* items,
						 UIMenuBar* bar, bool isSubmenu)
	: m_root(std::move(root)),
	  m_items(items ? items : m_root.get()),
	  m_bar(bar),
	  m_isSubmenu(isSubmenu) {
	m_interactive = true;
	setPadding(UIInsets::all(kPanelPadding));
}

float UIMenuPanel::itemHeight(const UIMenuItem& item) const {
	return item.isSeparator ? kSeparatorHeight : theme().rowHeight;
}

Vector2 UIMenuPanel::measureContent(Vector2 /*available*/) {
	float textWidth = 0.0f, shortcutWidth = 0.0f, height = 0.0f;
	for (const UIMenuItem& item : *m_items) {
		height += itemHeight(item);
		textWidth = std::max(textWidth,
							 ui::measureText(font(), item.text, theme().fontSize).x);
		shortcutWidth = std::max(
			shortcutWidth, ui::measureText(font(), item.shortcut, theme().fontSize).x);
	}
	const float width = kCheckColumn + textWidth +
						(shortcutWidth > 0.0f ? kShortcutGap + shortcutWidth : 0.0f) +
						kArrowColumn;
	return Vector2(std::max(width, kMinWidth), height);
}

UIRect UIMenuPanel::rowRect(size_t index) const {
	float y = getPadding().top;
	for (size_t i = 0; i < index && i < m_items->size(); i++) {
		y += itemHeight((*m_items)[i]);
	}
	const float h = index < m_items->size() ? itemHeight((*m_items)[index]) : 0.0f;
	return {Vector2(getPadding().left, y), Vector2(contentSize().x, h)};
}

size_t UIMenuPanel::rowAt(float localY) const {
	float y = getPadding().top;
	for (size_t i = 0; i < m_items->size(); i++) {
		const float h = itemHeight((*m_items)[i]);
		if (localY >= y && localY < y + h) return i;
		y += h;
	}
	return kNone;
}

size_t UIMenuPanel::nextSelectable(size_t from, int direction) const {
	const size_t count = m_items->size();
	if (count == 0) return kNone;
	// Walk `step` items away from `from` (or from an end when kNone),
	// wrapping, until a selectable item is found.
	for (size_t step = 1; step <= count; step++) {
		size_t index;
		if (from == kNone) {
			index = direction > 0 ? step - 1 : count - step;
		} else if (direction > 0) {
			index = (from + step) % count;
		} else {
			index = (from + count - step % count) % count;
		}
		if ((*m_items)[index].isSelectable()) return index;
	}
	return kNone;
}

void UIMenuPanel::setHighlighted(size_t index) {
	if (index != m_highlighted) {
		m_highlighted = index;
		markVisualDirty();
	}
}

void UIMenuPanel::highlightFirst() {
	setHighlighted(nextSelectable(kNone, +1));
}

UIMenuPanel* UIMenuPanel::openSubmenu(size_t index, bool fromKeyboard) {
	UIManager* manager = getManager();
	if (!manager || index >= m_items->size()) return nullptr;
	UIMenuItem& item = (*m_items)[index];
	if (item.children.empty() || !item.isSelectable()) return nullptr;
	if (m_child && m_childIndex == index) {
		if (fromKeyboard) m_child->highlightFirst();
		return m_child;
	}
	if (m_child) manager->closePopup(m_child);
	manager->layoutNow();
	const UIRect row = rowRect(index);
	const UIRect screen = getScreenRect();
	const Vector3 rowTop = getWorldMatrix() * Vector3(0.0f, row.position.y, 1.0f);
	const UIRect anchor{Vector2(screen.position.x, rowTop.y - kPanelPadding),
						Vector2(screen.size.x, row.size.y)};
	auto panel = std::make_unique<UIMenuPanel>(m_root, &item.children, m_bar, true);
	m_child = panel.get();
	m_childIndex = index;
	manager->openPopup(this, std::move(panel), anchor, UIPopupPlacement::Right);
	if (fromKeyboard) m_child->highlightFirst();
	m_pendingSubmenu = kNone;
	return m_child;
}

void UIMenuPanel::onPopupClosed(UIObject* popup) {
	if (popup == m_child) {
		m_child = nullptr;
		m_childIndex = kNone;
		markVisualDirty();
	}
}

void UIMenuPanel::activate(size_t index) {
	if (index >= m_items->size()) return;
	UIMenuItem& item = (*m_items)[index];
	if (!item.isSelectable()) return;
	if (!item.children.empty()) {
		openSubmenu(index, true);
		return;
	}
	if (item.checkable) item.checked = !item.checked;
	// Copy first: closing destroys this panel (deferred) and the action may
	// replace the menu data.
	const std::function<void()> action = item.onActivate;
	if (UIManager* manager = getManager()) manager->closeAllPopups();
	if (action) action();
}

bool UIMenuPanel::onMouseMove(const UIMouseEvent& event) {
	const size_t row = rowAt(toLocal(event.position).y);
	if (row == kNone || !(*m_items)[row].isSelectable()) return true;
	setHighlighted(row);
	if (!(*m_items)[row].children.empty()) {
		if (m_childIndex != row && m_pendingSubmenu != row) {
			m_pendingSubmenu = row;
			m_pendingTime = 0.0f;
			if (UIManager* manager = getManager()) manager->animate(this);
		}
	} else {
		m_pendingSubmenu = kNone;
		if (m_child) {
			if (UIManager* manager = getManager()) manager->closePopup(m_child);
		}
	}
	return true;
}

bool UIMenuPanel::tick(float delta) {
	if (m_pendingSubmenu == kNone) return false;
	m_pendingTime += delta;
	if (m_pendingTime < theme().submenuDelaySeconds) return true;
	openSubmenu(m_pendingSubmenu, false);
	m_pendingSubmenu = kNone;
	return false;
}

bool UIMenuPanel::onMouseUp(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Right &&
		event.button != UIMouseButton::Left) {
		return true;
	}
	if (containsPoint(event.position)) activate(rowAt(toLocal(event.position).y));
	return true;
}

bool UIMenuPanel::onKey(const UIKeyEvent& event) {
	UIManager* manager = getManager();
	switch (event.key) {
		case UIKey::Down:
			setHighlighted(nextSelectable(m_highlighted, +1));
			return true;
		case UIKey::Up:
			setHighlighted(nextSelectable(m_highlighted, -1));
			return true;
		case UIKey::Home:
			setHighlighted(nextSelectable(kNone, +1));
			return true;
		case UIKey::End:
			setHighlighted(nextSelectable(kNone, -1));
			return true;
		case UIKey::Right:
			if (m_highlighted != kNone &&
				!(*m_items)[m_highlighted].children.empty()) {
				openSubmenu(m_highlighted, true);
			} else if (m_bar) {
				m_bar->switchMenu(+1);
			}
			return true;
		case UIKey::Left:
			if (m_isSubmenu) {
				if (manager) manager->closePopup(this);
			} else if (m_bar) {
				m_bar->switchMenu(-1);
			}
			return true;
		case UIKey::Enter:
		case UIKey::Space:
			activate(m_highlighted);
			return true;
		case UIKey::Escape:
			if (manager) manager->closePopup(this);
			return true;
		default:
			return false;
	}
}

bool UIMenuPanel::onChar(uint32_t codepoint) {
	if (codepoint < 32 || codepoint > 126) return false;
	const char wanted =
		static_cast<char>(std::tolower(static_cast<int>(codepoint)));
	size_t index = m_highlighted;
	for (size_t step = 0; step < m_items->size(); step++) {
		index = nextSelectable(index, +1);
		if (index == kNone) break;
		const std::string& text = (*m_items)[index].text;
		if (!text.empty() &&
			std::tolower(static_cast<unsigned char>(text[0])) == wanted) {
			setHighlighted(index);
			break;
		}
	}
	return true;
}

void UIMenuPanel::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.shadow(bounds, t.popupRadius, t.shadowBlur, t.shadowColor);
	list.roundedRect(bounds, t.popupColor, t.popupRadius, t.borderWidth,
					 t.borderColor);
	for (size_t i = 0; i < m_items->size(); i++) {
		const UIMenuItem& item = (*m_items)[i];
		const UIRect row = rowRect(i);
		if (item.isSeparator) {
			const float y = row.position.y + row.size.y * 0.5f;
			list.line(Vector2(row.position.x + 6.0f, y),
					  Vector2(row.max().x - 6.0f, y), 1.0f, t.separatorColor);
			continue;
		}
		const bool highlighted = i == m_highlighted || i == m_childIndex;
		if (highlighted && item.enabled) {
			list.roundedRect(row, t.menuHighlightColor, t.controlRadius);
		}
		const Vector4 color = item.enabled ? t.textColor : t.disabledText;
		if (item.checkable && item.checked) {
			const Vector2 o = row.position + Vector2(6.0f, row.size.y * 0.5f - 6.0f);
			list.line(o + Vector2(1.0f, 6.0f), o + Vector2(4.5f, 9.5f), 1.6f, color);
			list.line(o + Vector2(4.5f, 9.5f), o + Vector2(11.0f, 2.0f), 1.6f, color);
		}
		ui::drawTextCenteredY(list, font(), item.text, t.fontSize,
							  row.position.x + kCheckColumn, row, color);
		if (!item.shortcut.empty()) {
			const float w = ui::measureText(font(), item.shortcut, t.fontSize).x;
			ui::drawTextCenteredY(list, font(), item.shortcut, t.fontSize,
								  row.max().x - kArrowColumn - w, row,
								  highlighted ? t.textColor : t.mutedTextColor);
		}
		if (!item.children.empty()) {
			const Vector2 c(row.max().x - kArrowColumn * 0.5f,
							row.position.y + row.size.y * 0.5f);
			list.triangle(c + Vector2(-2.5f, -4.5f), c + Vector2(-2.5f, 4.5f),
						  c + Vector2(2.5f, 0.0f), color);
		}
	}
}

}  // namespace bird
