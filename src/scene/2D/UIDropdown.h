#ifndef BIRD_UIDROPDOWN_H
#define BIRD_UIDROPDOWN_H

#include <functional>
#include <string>
#include <vector>

#include "UIObject.h"

namespace bird {

class UIDropdownList;

/**
 * Combo box: shows the selected item; clicking opens a popup list (on the
 * overlay, so it is never clipped by the dropdown's parents).
 *
 * Closed and focused: Up/Down change the selection, Enter/Space open the
 * list, typing a letter selects the next item starting with it. Open: see
 * UIDropdownList.
 */
class UIDropdown : public UIObject {
   public:
	static constexpr size_t kNone = static_cast<size_t>(-1);
	static constexpr size_t kMaxVisibleRows = 8;

	explicit UIDropdown(std::vector<std::string> items = {});

	UIDropdown& setItems(std::vector<std::string> items);
	const std::vector<std::string>& getItems() const { return m_items; }
	/**
	 * Selects an item (kNone clears); notifies onChanged on change.
	 * @throws std::out_of_range for an invalid index.
	 */
	UIDropdown& setSelectedIndex(size_t index);
	size_t getSelectedIndex() const { return m_selected; }
	/** @return the selected text, or "" when nothing is selected. */
	std::string getSelectedText() const;
	UIDropdown& setPlaceholder(std::string placeholder);
	UIDropdown& setOnChanged(std::function<void(size_t)> onChanged);

	void open();
	void close();
	bool isOpen() const { return m_list != nullptr; }
	UIDropdownList* getList() const { return m_list; }
	/** Next item after `from` (wrapping) starting with a letter, or kNone. */
	size_t findByInitial(char initial, size_t from) const;

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	void onPopupClosed(UIObject* popup) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	std::vector<std::string> m_items;
	size_t m_selected = kNone;
	std::string m_placeholder = "Select...";
	std::function<void(size_t)> m_onChanged;
	UIDropdownList* m_list = nullptr;
};

/**
 * Popup list of a UIDropdown. Hover or Up/Down/Home/End move the
 * highlight; click or Enter selects and closes; Escape closes; typing a
 * letter jumps to the next matching item. Scrolls when there are more
 * than UIDropdown::kMaxVisibleRows items.
 */
class UIDropdownList : public UIObject {
   public:
	explicit UIDropdownList(UIDropdown& owner);

	size_t getHighlighted() const { return m_highlighted; }
	void setHighlighted(size_t index);
	/** Selects the highlighted item in the owner and closes. */
	void choose(size_t index);
	float getScrollOffset() const { return m_scroll; }
	/** Row index under a local y, or UIDropdown::kNone. */
	size_t rowAt(float localY) const;

	bool onMouseDown(const UIMouseEvent& event) override { return true; }
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onScroll(const UIScrollEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	float rowHeight() const { return theme().rowHeight; }
	float maxScroll() const;
	void scrollIntoView(size_t index);

	UIDropdown& m_owner;
	size_t m_highlighted;
	float m_scroll = 0.0f;
};

}  // namespace bird

#endif	// !BIRD_UIDROPDOWN_H
