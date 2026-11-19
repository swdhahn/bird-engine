#include "UIRowView.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "UIDrawList.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 220.0f;
constexpr float kScrollbarWidth = 6.0f;
constexpr float kMinThumb = 16.0f;
constexpr int kWheelRows = 3;
}  // namespace

UIRowView::UIRowView() {
	m_interactive = true;
	m_clipChildren = true;
	setPadding(UIInsets::all(3.0f));
}

UIRowView& UIRowView::setVisibleRows(int rows) {
	m_visibleRows = std::max(rows, 1);
	markLayoutDirty();
	return *this;
}

Vector2 UIRowView::measureContent(Vector2 /*available*/) {
	return Vector2(kDefaultWidth, static_cast<float>(m_visibleRows) * rowHeight());
}

float UIRowView::scrollbarLane() const {
	return getMaxScroll() > 0.0f ? kScrollbarWidth + 2.0f : 0.0f;
}

float UIRowView::getMaxScroll() const {
	const float total = static_cast<float>(rowCount()) * rowHeight();
	return std::max(total - contentSize().y, 0.0f);
}

void UIRowView::setScrollOffset(float offset) {
	const float clamped = std::clamp(offset, 0.0f, getMaxScroll());
	if (clamped != m_scroll) {
		m_scroll = clamped;
		markVisualDirty();
	}
}

void UIRowView::scrollToRow(size_t row) {
	if (row >= rowCount()) return;
	const float viewport0 = contentSize().y;
	if (viewport0 <= 0.0f) {
		m_pendingScrollRow = row;  // applied after the first layout
		return;
	}
	const float top = static_cast<float>(row) * rowHeight();
	const float viewport = contentSize().y;
	float scroll = m_scroll;
	if (top < scroll) scroll = top;
	if (top + rowHeight() > scroll + viewport) scroll = top + rowHeight() - viewport;
	setScrollOffset(scroll);
}

size_t UIRowView::rowAt(float localY) const {
	const float y = localY - getPadding().top + m_scroll;
	if (y < 0.0f || localY > getSize().y - getPadding().bottom) return kNone;
	const auto row = static_cast<size_t>(y / rowHeight());
	return row < rowCount() ? row : kNone;
}

UIRect UIRowView::rowRect(size_t row) const {
	const float y = getPadding().top + static_cast<float>(row) * rowHeight() - m_scroll;
	return {Vector2(getPadding().left, y),
			Vector2(contentSize().x - scrollbarLane(), rowHeight())};
}

void UIRowView::arrangeChildren() {
	UIObject::arrangeChildren();
	if (m_pendingScrollRow != kNone) {
		const size_t row = m_pendingScrollRow;
		m_pendingScrollRow = kNone;
		scrollToRow(row);
	}
}

UIRect UIRowView::childClipRect() const {
	return {getPadding().start(), contentSize()};
}

void UIRowView::setCursorSilently(size_t row) {
	m_cursor = (row == kNone || rowCount() == 0) ? kNone
												 : std::min(row, rowCount() - 1);
	markVisualDirty();
}

void UIRowView::moveCursor(size_t row, bool extend, bool toggle) {
	if (rowCount() == 0) return;
	m_cursor = std::min(row, rowCount() - 1);
	scrollToRow(m_cursor);
	markVisualDirty();
	onCursorMoved(m_cursor, extend, toggle);
}

UIRect UIRowView::thumbRect() const {
	const float viewport = contentSize().y;
	const float maxScroll = getMaxScroll();
	const float height =
		std::max(viewport * viewport / (viewport + maxScroll), kMinThumb);
	const float y = getPadding().top +
					(maxScroll > 0.0f ? (viewport - height) * (m_scroll / maxScroll)
									  : 0.0f);
	return {Vector2(getSize().x - getPadding().right - kScrollbarWidth, y),
			Vector2(kScrollbarWidth, height)};
}

bool UIRowView::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	const Vector2 local = toLocal(event.position);
	if (getMaxScroll() > 0.0f &&
		local.x >= getSize().x - getPadding().right - scrollbarLane()) {
		const UIRect thumb = thumbRect();
		if (local.y >= thumb.position.y && local.y <= thumb.max().y) {
			m_draggingThumb = true;
			m_dragStartY = event.position.y;
			m_dragStartScroll = m_scroll;
		} else {
			const float page = contentSize().y * 0.9f;
			setScrollOffset(m_scroll + (local.y < thumb.position.y ? -page : page));
		}
		return true;
	}
	const size_t row = rowAt(local.y);
	if (row == kNone) return true;
	const UIRect rect = rowRect(row);
	if (onRowPressed(row, local - rect.position, event)) return true;
	moveCursor(row, event.modifiers & UI_MOD_SHIFT,
			   event.modifiers & UI_MOD_CONTROL);
	if (event.clickCount == 2) onRowActivated(row);
	return true;
}

bool UIRowView::onMouseMove(const UIMouseEvent& event) {
	if (!m_draggingThumb) return false;
	const float travel = contentSize().y - thumbRect().size.y;
	if (travel > 0.0f) {
		setScrollOffset(m_dragStartScroll +
						(event.position.y - m_dragStartY) / travel * getMaxScroll());
	}
	return true;
}

bool UIRowView::onMouseUp(const UIMouseEvent& /*event*/) {
	m_draggingThumb = false;
	return true;
}

bool UIRowView::onScroll(const UIScrollEvent& event) {
	const float before = m_scroll;
	setScrollOffset(m_scroll - event.delta.y * kWheelRows * rowHeight());
	return m_scroll != before;	// at a limit, let outer views scroll
}

bool UIRowView::onKey(const UIKeyEvent& event) {
	if (onRowKey(event)) return true;
	const size_t count = rowCount();
	if (count == 0) return false;
	const bool extend = event.shift();
	const bool toggle = event.control();
	const size_t page = std::max<size_t>(
		static_cast<size_t>(contentSize().y / rowHeight()), 1);
	const size_t cursor = m_cursor == kNone ? 0 : m_cursor;
	switch (event.key) {
		case UIKey::Up:
			moveCursor(m_cursor == kNone || cursor == 0 ? 0 : cursor - 1, extend, toggle);
			return true;
		case UIKey::Down:
			moveCursor(m_cursor == kNone ? 0 : cursor + 1, extend, toggle);
			return true;
		case UIKey::PageUp:
			moveCursor(cursor > page ? cursor - page : 0, extend, toggle);
			return true;
		case UIKey::PageDown:
			moveCursor(cursor + page, extend, toggle);
			return true;
		case UIKey::Home:
			moveCursor(0, extend, toggle);
			return true;
		case UIKey::End:
			moveCursor(count - 1, extend, toggle);
			return true;
		case UIKey::Enter:
			if (m_cursor != kNone) onRowActivated(m_cursor);
			return true;
		default:
			return false;
	}
}

bool UIRowView::onChar(uint32_t codepoint) {
	if (codepoint <= 32 || codepoint > 126) return false;
	const size_t count = rowCount();
	const auto wanted = static_cast<char>(std::tolower(static_cast<int>(codepoint)));
	const size_t start = m_cursor == kNone ? 0 : m_cursor + 1;
	// Next row (wrapping) whose text starts with the typed letter: O(n).
	for (size_t i = 0; i < count; i++) {
		const size_t row = (start + i) % count;
		const std::string text = rowText(row);
		if (!text.empty() &&
			std::tolower(static_cast<unsigned char>(text[0])) == wanted) {
			moveCursor(row, false, false);
			return true;
		}
	}
	return true;
}

void UIRowView::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.roundedRect(bounds, t.inputColor, t.controlRadius, t.borderWidth,
					 isFocused() ? t.focusBorderColor : t.borderColor);
	list.pushClip(childClipRect());
	const size_t count = rowCount();
	const float viewportBottom = getSize().y - getPadding().bottom;
	// Virtualization: start at the first visible row, stop below the view.
	for (size_t row = static_cast<size_t>(m_scroll / rowHeight()); row < count; row++) {
		const UIRect rect = rowRect(row);
		if (rect.position.y > viewportBottom) break;
		drawRow(list, row, rect);
		if (row == m_cursor && isFocused()) {
			list.roundedRect(rect, Vector4(0.0f), t.controlRadius, 1.0f,
							 t.focusBorderColor);
		}
	}
	list.popClip();
	if (getMaxScroll() > 0.0f) {
		const UIRect thumb = thumbRect();
		list.roundedRect(thumb, t.scrollbarColor, thumb.size.x * 0.5f);
	}
}

}  // namespace bird
