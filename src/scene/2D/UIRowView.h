#ifndef BIRD_UIROWVIEW_H
#define BIRD_UIROWVIEW_H

#include <string>

#include "UIObject.h"

namespace bird {

/**
 * Base for virtualized, fixed-height row views (UIListView, UITreeView).
 *
 * Only rows inside the viewport are drawn, so drawing is O(visible rows)
 * regardless of the row count. Provides scrolling (wheel, draggable
 * scrollbar, track paging), a keyboard cursor row (Up/Down, PageUp/
 * PageDown, Home/End, Shift/Ctrl passed through), Enter/double-click
 * activation and type-ahead on the row text.
 */
class UIRowView : public UIObject {
   public:
	static constexpr size_t kNone = static_cast<size_t>(-1);

	UIRowView();

	/** Natural height in rows (the view scrolls beyond it). */
	UIRowView& setVisibleRows(int rows);
	float rowHeight() const { return theme().rowHeight; }
	/** Row under a local y coordinate, or kNone. */
	size_t rowAt(float localY) const;
	/** Local rect of a row at the current scroll offset. */
	UIRect rowRect(size_t row) const;

	size_t getCursorRow() const { return m_cursor; }
	float getScrollOffset() const { return m_scroll; }
	float getMaxScroll() const;
	void setScrollOffset(float offset);
	/** Scrolls the minimum amount that makes a row fully visible. */
	void scrollToRow(size_t row);

	bool isFocusable() const override { return true; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onScroll(const UIScrollEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	UIRect childClipRect() const override;
	void draw(UIDrawList& list) override;

   protected:
	virtual size_t rowCount() const = 0;
	virtual std::string rowText(size_t row) const = 0;
	/** Draws one row; rect is in local space. */
	virtual void drawRow(UIDrawList& list, size_t row, const UIRect& rect) = 0;
	/**
	 * The cursor moved to `row` by mouse or keyboard; subclasses update
	 * their selection. `extend` = Shift held, `toggle` = Ctrl held.
	 */
	virtual void onCursorMoved(size_t row, bool extend, bool toggle) = 0;
	/** A press on a row, before cursor handling; return true to consume. */
	virtual bool onRowPressed(size_t row, Vector2 localInRow,
							  const UIMouseEvent& event) {
		return false;
	}
	/** Enter or double-click on a row. */
	virtual void onRowActivated(size_t row) {}
	/** Extra keys (e.g. tree Left/Right); return true when handled. */
	virtual bool onRowKey(const UIKeyEvent& event) { return false; }
	Vector2 measureContent(Vector2 available) override;
	void arrangeChildren() override;

	/** Moves the cursor (clamped), scrolls to it and notifies. */
	void moveCursor(size_t row, bool extend, bool toggle);
	/** Sets the cursor without notifying (e.g. after a model change). */
	void setCursorSilently(size_t row);
	float scrollbarLane() const;

   private:
	UIRect thumbRect() const;

	size_t m_cursor = kNone;
	float m_scroll = 0.0f;
	int m_visibleRows = 8;
	size_t m_pendingScrollRow = kNone;
	bool m_draggingThumb = false;
	float m_dragStartY = 0.0f;
	float m_dragStartScroll = 0.0f;
};

}  // namespace bird

#endif	// !BIRD_UIROWVIEW_H
