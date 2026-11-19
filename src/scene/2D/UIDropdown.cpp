#include "UIDropdown.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "UIManager.h"
#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kChevronWidth = 20.0f;
constexpr float kListPadding = 4.0f;

char lower(char c) {
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
}  // namespace

// ------------------------------------------------------------ UIDropdown

UIDropdown::UIDropdown(std::vector<std::string> items)
	: m_items(std::move(items)) {
	m_interactive = true;
	setPadding(UIInsets{theme().padding, theme().padding * 0.5f,
						theme().padding + kChevronWidth, theme().padding * 0.5f});
	UISizeRule height = UISizeRule::fit();
	height.min = theme().controlHeight;
	setHeight(height);
}

UIDropdown& UIDropdown::setItems(std::vector<std::string> items) {
	close();
	m_items = std::move(items);
	m_selected = kNone;
	markLayoutDirty();
	return *this;
}

UIDropdown& UIDropdown::setSelectedIndex(size_t index) {
	if (index != kNone && index >= m_items.size()) {
		throw std::out_of_range("UIDropdown::setSelectedIndex: no such item");
	}
	if (index != m_selected) {
		m_selected = index;
		markVisualDirty();
		if (m_onChanged) m_onChanged(m_selected);
	}
	return *this;
}

std::string UIDropdown::getSelectedText() const {
	return m_selected == kNone ? std::string() : m_items[m_selected];
}

UIDropdown& UIDropdown::setPlaceholder(std::string placeholder) {
	m_placeholder = std::move(placeholder);
	markLayoutDirty();
	return *this;
}

UIDropdown& UIDropdown::setOnChanged(std::function<void(size_t)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

void UIDropdown::open() {
	UIManager* manager = getManager();
	if (!manager || m_list || m_items.empty() || !isEffectivelyEnabled()) return;
	manager->layoutNow();
	auto list = std::make_unique<UIDropdownList>(*this);
	m_list = list.get();
	manager->openPopup(this, std::move(list), getScreenRect());
	manager->setFocus(this);
	markVisualDirty();
}

void UIDropdown::close() {
	if (UIManager* manager = getManager(); manager && m_list) {
		manager->closePopup(m_list);  // onPopupClosed clears m_list
	}
}

void UIDropdown::onPopupClosed(UIObject* popup) {
	if (popup == m_list) {
		m_list = nullptr;
		markVisualDirty();
	}
}

size_t UIDropdown::findByInitial(char initial, size_t from) const {
	if (m_items.empty()) return kNone;
	const size_t count = m_items.size();
	const size_t start = from == kNone ? 0 : from + 1;
	for (size_t i = 0; i < count; i++) {
		const size_t index = (start + i) % count;
		if (!m_items[index].empty() &&
			lower(m_items[index][0]) == lower(initial)) {
			return index;
		}
	}
	return kNone;
}

bool UIDropdown::onMouseDown(const UIMouseEvent& event) {
	return event.button == UIMouseButton::Left;
}

bool UIDropdown::onMouseUp(const UIMouseEvent& event) {
	// The press that closes an open list is consumed by light dismiss, so
	// reaching here always means "open".
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		open();
	}
	return true;
}

bool UIDropdown::onKey(const UIKeyEvent& event) {
	if (m_items.empty()) return false;
	const size_t last = m_items.size() - 1;
	switch (event.key) {
		case UIKey::Enter:
		case UIKey::Space:
			open();
			return true;
		case UIKey::Up:
			setSelectedIndex(m_selected == kNone || m_selected == 0
								 ? 0
								 : m_selected - 1);
			return true;
		case UIKey::Down:
			setSelectedIndex(m_selected == kNone ? 0
												 : std::min(m_selected + 1, last));
			return true;
		case UIKey::Home:
			setSelectedIndex(0);
			return true;
		case UIKey::End:
			setSelectedIndex(last);
			return true;
		default:
			return false;
	}
}

bool UIDropdown::onChar(uint32_t codepoint) {
	if (codepoint < 32 || codepoint > 126) return false;
	const size_t match = findByInitial(static_cast<char>(codepoint), m_selected);
	if (match != kNone) setSelectedIndex(match);
	return true;
}

Vector2 UIDropdown::measureContent(Vector2 /*available*/) {
	Vector2 size = ui::measureText(font(), m_placeholder, theme().fontSize);
	for (const std::string& item : m_items) {
		size = glm::max(size, ui::measureText(font(), item, theme().fontSize));
	}
	return size;
}

void UIDropdown::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	const Vector4 fill = !isEffectivelyEnabled() ? t.disabledColor
						 : isPressed()			 ? t.activeColor
						 : isHovered()			 ? t.hoverColor
												 : t.surfaceColor;
	list.roundedRect(bounds, fill, t.controlRadius, t.borderWidth,
					 isFocused() || isOpen() ? t.focusBorderColor : t.borderColor);
	const bool placeholder = m_selected == kNone;
	list.pushClip({getPadding().start(), contentSize()});
	ui::drawTextCenteredY(list, font(), placeholder ? m_placeholder : m_items[m_selected],
						  t.fontSize, getPadding().left, bounds,
						  !isEffectivelyEnabled() ? t.disabledText
						  : placeholder			  ? t.mutedTextColor
												  : t.textColor);
	list.popClip();
	// Chevron, pointing up while the list is open.
	const Vector2 c(getSize().x - t.padding - kChevronWidth * 0.5f,
					getSize().y * 0.5f);
	const float w = 4.5f, h = 2.5f;
	const Vector4 arrow = isEffectivelyEnabled() ? t.mutedTextColor : t.disabledText;
	if (isOpen()) {
		list.triangle(c + Vector2(-w, h), c + Vector2(w, h), c + Vector2(0, -h), arrow);
	} else {
		list.triangle(c + Vector2(-w, -h), c + Vector2(0, h), c + Vector2(w, -h), arrow);
	}
}

// -------------------------------------------------------- UIDropdownList

UIDropdownList::UIDropdownList(UIDropdown& owner)
	: m_owner(owner),
	  m_highlighted(owner.getSelectedIndex() == UIDropdown::kNone
						? 0
						: owner.getSelectedIndex()) {
	m_interactive = true;
	m_clipChildren = true;
	setPadding(UIInsets::all(kListPadding));
	setWidth(UISizeRule::fixed(std::max(owner.getSize().x, 80.0f)));
	scrollIntoView(m_highlighted);	// open with the selection visible
}

Vector2 UIDropdownList::measureContent(Vector2 /*available*/) {
	const size_t visible =
		std::min(m_owner.getItems().size(), UIDropdown::kMaxVisibleRows);
	return Vector2(0.0f, static_cast<float>(visible) * rowHeight());
}

float UIDropdownList::maxScroll() const {
	const float total =
		static_cast<float>(m_owner.getItems().size()) * rowHeight();
	return std::max(total - contentSize().y, 0.0f);
}

size_t UIDropdownList::rowAt(float localY) const {
	const float y = localY - getPadding().top + m_scroll;
	if (y < 0.0f) return UIDropdown::kNone;
	const auto row = static_cast<size_t>(y / rowHeight());
	return row < m_owner.getItems().size() ? row : UIDropdown::kNone;
}

void UIDropdownList::setHighlighted(size_t index) {
	if (index >= m_owner.getItems().size()) return;
	if (index != m_highlighted) {
		m_highlighted = index;
		markVisualDirty();
	}
	scrollIntoView(index);
}

void UIDropdownList::scrollIntoView(size_t index) {
	const float top = static_cast<float>(index) * rowHeight();
	const float viewport = contentSize().y;
	if (viewport <= 0.0f) {
		// Not laid out yet: scroll so the row starts the list.
		m_scroll = std::max(top - rowHeight() * 3.0f, 0.0f);
	} else if (top < m_scroll) {
		m_scroll = top;
	} else if (top + rowHeight() > m_scroll + viewport) {
		m_scroll = top + rowHeight() - viewport;
	}
	m_scroll = std::clamp(m_scroll, 0.0f, std::max(maxScroll(), 0.0f));
	markVisualDirty();
}

void UIDropdownList::choose(size_t index) {
	if (index >= m_owner.getItems().size()) return;
	UIDropdown& owner = m_owner;  // this list is destroyed by close()
	owner.close();
	owner.setSelectedIndex(index);
}

bool UIDropdownList::onMouseMove(const UIMouseEvent& event) {
	const size_t row = rowAt(toLocal(event.position).y);
	if (row != UIDropdown::kNone && row != m_highlighted) {
		m_highlighted = row;
		markVisualDirty();
	}
	return true;
}

bool UIDropdownList::onMouseUp(const UIMouseEvent& event) {
	if (event.button == UIMouseButton::Left && containsPoint(event.position)) {
		choose(rowAt(toLocal(event.position).y));
	}
	return true;
}

bool UIDropdownList::onScroll(const UIScrollEvent& event) {
	m_scroll = std::clamp(m_scroll - event.delta.y * rowHeight(), 0.0f,
						  maxScroll());
	markVisualDirty();
	return true;
}

bool UIDropdownList::onKey(const UIKeyEvent& event) {
	const size_t count = m_owner.getItems().size();
	if (count == 0) return false;
	switch (event.key) {
		case UIKey::Up:
			setHighlighted(m_highlighted == 0 ? 0 : m_highlighted - 1);
			return true;
		case UIKey::Down:
			setHighlighted(std::min(m_highlighted + 1, count - 1));
			return true;
		case UIKey::PageUp:
			setHighlighted(m_highlighted > UIDropdown::kMaxVisibleRows
							   ? m_highlighted - UIDropdown::kMaxVisibleRows
							   : 0);
			return true;
		case UIKey::PageDown:
			setHighlighted(
				std::min(m_highlighted + UIDropdown::kMaxVisibleRows, count - 1));
			return true;
		case UIKey::Home:
			setHighlighted(0);
			return true;
		case UIKey::End:
			setHighlighted(count - 1);
			return true;
		case UIKey::Enter:
		case UIKey::Space:
			choose(m_highlighted);
			return true;
		case UIKey::Escape:
			m_owner.close();
			return true;
		default:
			return false;
	}
}

bool UIDropdownList::onChar(uint32_t codepoint) {
	if (codepoint < 32 || codepoint > 126) return false;
	const size_t match =
		m_owner.findByInitial(static_cast<char>(codepoint), m_highlighted);
	if (match != UIDropdown::kNone) setHighlighted(match);
	return true;
}

void UIDropdownList::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.shadow(bounds, t.popupRadius, t.shadowBlur, t.shadowColor);
	list.roundedRect(bounds, t.popupColor, t.popupRadius, t.borderWidth,
					 t.borderColor);

	const UIRect inner{getPadding().start(), contentSize()};
	list.pushClip(inner);
	const auto& items = m_owner.getItems();
	const float rowH = rowHeight();
	const size_t first = static_cast<size_t>(m_scroll / rowH);
	for (size_t i = first; i < items.size(); i++) {
		const float y = inner.position.y + static_cast<float>(i) * rowH - m_scroll;
		if (y > inner.max().y) break;  // rows below the viewport: done
		const UIRect row{Vector2(inner.position.x, y), Vector2(inner.size.x, rowH)};
		if (i == m_highlighted) {
			list.roundedRect(row, t.menuHighlightColor, t.controlRadius);
		} else if (i == m_owner.getSelectedIndex()) {
			list.roundedRect(row, t.hoverColor, t.controlRadius);
		}
		ui::drawTextCenteredY(list, font(), items[i], t.fontSize,
							  row.position.x + t.padding, row, t.textColor);
	}
	list.popClip();

	if (maxScroll() > 0.0f) {
		const float viewport = inner.size.y;
		const float total = viewport + maxScroll();
		const float thumbH = std::max(viewport * viewport / total, 16.0f);
		const float thumbY =
			inner.position.y + (viewport - thumbH) * (m_scroll / maxScroll());
		const float w = 4.0f;
		list.roundedRect({Vector2(getSize().x - w - 2.0f, thumbY), Vector2(w, thumbH)},
						 t.scrollbarColor, w * 0.5f);
	}
}

}  // namespace bird
