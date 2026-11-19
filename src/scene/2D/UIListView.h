#ifndef BIRD_UILISTVIEW_H
#define BIRD_UILISTVIEW_H

#include <functional>
#include <string>
#include <vector>

#include "UIRowView.h"

namespace bird {

enum class UISelectionMode : uint8_t { Single, Multiple };

/**
 * Virtualized list of text rows. Items come from a vector or, for large or
 * generated data, from a count plus a provider function, so a million rows
 * cost nothing until they scroll into view.
 *
 * Single mode: click or arrows select one row. Multiple mode: Ctrl+click
 * toggles, Shift+click/Shift+arrows select a range from the anchor,
 * Ctrl+A selects all, Space toggles the cursor row. Enter or double click
 * activates a row.
 */
class UIListView : public UIRowView {
   public:
	using Provider = std::function<std::string(size_t)>;

	explicit UIListView(std::vector<std::string> items = {});

	UIListView& setItems(std::vector<std::string> items);
	/** Generated items: text for index i is provider(i). */
	UIListView& setItemProvider(size_t count, Provider provider);
	size_t getItemCount() const { return m_count; }
	std::string getItemText(size_t index) const;

	UIListView& setSelectionMode(UISelectionMode mode);
	UISelectionMode getSelectionMode() const { return m_mode; }
	bool isSelected(size_t index) const;
	/** Selected indices in ascending order. O(n) in the item count. */
	std::vector<size_t> getSelection() const;
	/** Single-mode convenience: the selected index or kNone. */
	size_t getSelectedIndex() const;
	/** Replaces the selection with one row (kNone clears it). */
	UIListView& select(size_t index);
	void clearSelection();
	void selectAll();

	UIListView& setOnSelectionChanged(std::function<void()> onChanged);
	UIListView& setOnActivate(std::function<void(size_t)> onActivate);

	bool onKey(const UIKeyEvent& event) override;

   protected:
	size_t rowCount() const override { return m_count; }
	std::string rowText(size_t row) const override { return getItemText(row); }
	void drawRow(UIDrawList& list, size_t row, const UIRect& rect) override;
	void onCursorMoved(size_t row, bool extend, bool toggle) override;
	void onRowActivated(size_t row) override;

   private:
	void resetSelection();
	void notify();

	std::vector<std::string> m_items;  // used when no provider is set
	Provider m_provider;
	size_t m_count = 0;
	UISelectionMode m_mode = UISelectionMode::Single;
	std::vector<bool> m_selected;  // one bit per item
	size_t m_anchor = kNone;	   // range-selection origin
	std::function<void()> m_onSelectionChanged;
	std::function<void(size_t)> m_onActivate;
};

}  // namespace bird

#endif	// !BIRD_UILISTVIEW_H
