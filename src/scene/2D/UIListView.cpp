#include "UIListView.h"

#include <algorithm>
#include <stdexcept>

#include "UITextUtil.h"

namespace bird {

UIListView::UIListView(std::vector<std::string> items) {
	setItems(std::move(items));
}

UIListView& UIListView::setItems(std::vector<std::string> items) {
	m_items = std::move(items);
	m_provider = nullptr;
	m_count = m_items.size();
	resetSelection();
	return *this;
}

UIListView& UIListView::setItemProvider(size_t count, Provider provider) {
	if (!provider) throw std::invalid_argument("UIListView: null provider");
	m_items.clear();
	m_provider = std::move(provider);
	m_count = count;
	resetSelection();
	return *this;
}

void UIListView::resetSelection() {
	m_selected.assign(m_count, false);
	m_anchor = kNone;
	setCursorSilently(kNone);
	setScrollOffset(0.0f);
	markLayoutDirty();
}

std::string UIListView::getItemText(size_t index) const {
	if (index >= m_count) return {};
	return m_provider ? m_provider(index) : m_items[index];
}

UIListView& UIListView::setSelectionMode(UISelectionMode mode) {
	m_mode = mode;
	if (mode == UISelectionMode::Single && getSelection().size() > 1) {
		select(getCursorRow());
	}
	return *this;
}

bool UIListView::isSelected(size_t index) const {
	return index < m_count && m_selected[index];
}

std::vector<size_t> UIListView::getSelection() const {
	std::vector<size_t> selection;
	for (size_t i = 0; i < m_count; i++) {
		if (m_selected[i]) selection.push_back(i);
	}
	return selection;
}

size_t UIListView::getSelectedIndex() const {
	for (size_t i = 0; i < m_count; i++) {
		if (m_selected[i]) return i;
	}
	return kNone;
}

UIListView& UIListView::select(size_t index) {
	std::fill(m_selected.begin(), m_selected.end(), false);
	if (index < m_count) {
		m_selected[index] = true;
		m_anchor = index;
		setCursorSilently(index);
		scrollToRow(index);
	}
	notify();
	return *this;
}

void UIListView::clearSelection() {
	std::fill(m_selected.begin(), m_selected.end(), false);
	notify();
}

void UIListView::selectAll() {
	if (m_mode != UISelectionMode::Multiple) return;
	std::fill(m_selected.begin(), m_selected.end(), true);
	notify();
}

UIListView& UIListView::setOnSelectionChanged(std::function<void()> onChanged) {
	m_onSelectionChanged = std::move(onChanged);
	return *this;
}

UIListView& UIListView::setOnActivate(std::function<void(size_t)> onActivate) {
	m_onActivate = std::move(onActivate);
	return *this;
}

void UIListView::notify() {
	markVisualDirty();
	if (m_onSelectionChanged) m_onSelectionChanged();
}

void UIListView::onCursorMoved(size_t row, bool extend, bool toggle) {
	if (m_mode == UISelectionMode::Single) {
		std::fill(m_selected.begin(), m_selected.end(), false);
		m_selected[row] = true;
		m_anchor = row;
	} else if (extend && m_anchor != kNone) {
		// Range from the anchor; Ctrl+Shift adds the range to the selection.
		if (!toggle) std::fill(m_selected.begin(), m_selected.end(), false);
		const size_t lo = std::min(m_anchor, row), hi = std::max(m_anchor, row);
		std::fill(m_selected.begin() + static_cast<long>(lo),
				  m_selected.begin() + static_cast<long>(hi) + 1, true);
	} else if (toggle) {
		m_selected[row] = !m_selected[row];
		m_anchor = row;
	} else {
		std::fill(m_selected.begin(), m_selected.end(), false);
		m_selected[row] = true;
		m_anchor = row;
	}
	notify();
}

void UIListView::onRowActivated(size_t row) {
	if (m_onActivate) m_onActivate(row);
}

bool UIListView::onKey(const UIKeyEvent& event) {
	if (m_mode == UISelectionMode::Multiple) {
		if (event.key == UIKey::A && event.control()) {
			selectAll();
			return true;
		}
		if (event.key == UIKey::Space && getCursorRow() != kNone) {
			const size_t row = getCursorRow();
			m_selected[row] = !m_selected[row];
			m_anchor = row;
			notify();
			return true;
		}
		// Ctrl+arrows move the cursor without changing the selection.
		if (event.control() && !event.shift() &&
			(event.key == UIKey::Up || event.key == UIKey::Down)) {
			const size_t cursor = getCursorRow() == kNone ? 0 : getCursorRow();
			const size_t next = event.key == UIKey::Up
									? (cursor == 0 ? 0 : cursor - 1)
									: std::min(cursor + 1, m_count - 1);
			setCursorSilently(next);
			scrollToRow(next);
			return true;
		}
	}
	return UIRowView::onKey(event);
}

void UIListView::drawRow(UIDrawList& list, size_t row, const UIRect& rect) {
	const UITheme& t = theme();
	if (m_selected[row]) {
		list.roundedRect(rect, isFocused() ? t.menuHighlightColor : t.activeColor,
						 t.controlRadius);
	}
	ui::drawTextCenteredY(list, font(), getItemText(row), t.fontSize,
						  rect.position.x + t.padding, rect, t.textColor);
}

}  // namespace bird
