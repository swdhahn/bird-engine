#ifndef BIRD_UIMENUPANEL_H
#define BIRD_UIMENUPANEL_H

#include <vector>

#include "UIMenu.h"
#include "UIObject.h"

namespace bird {

class UIMenuBar;

/**
 * Popup listing UIMenuItems: context menus, menu bar menus and submenus.
 *
 * Mouse: hovering highlights; hovering an item with children opens its
 * submenu after UITheme::submenuDelaySeconds; releasing on an item
 * activates it. Keys: Up/Down (skipping separators and disabled items),
 * Home/End, Right opens a submenu (or the next menu bar menu), Left closes
 * a submenu (or moves to the previous menu bar menu), Enter/Space
 * activates, Escape closes this level, letters jump to matching items.
 * Activating an action closes every popup, then runs it.
 */
class UIMenuPanel : public UIObject {
   public:
	static constexpr size_t kNone = static_cast<size_t>(-1);

	/**
	 * @param root - keeps the menu data alive while any level is open.
	 * @param items - this level (defaults to *root).
	 * @param bar - owning menu bar, for Left/Right switching; may be null.
	 */
	explicit UIMenuPanel(UIMenuItems root,
						 std::vector<UIMenuItem>* items = nullptr,
						 UIMenuBar* bar = nullptr, bool isSubmenu = false);

	size_t getHighlighted() const { return m_highlighted; }
	/** Highlights the first selectable item (keyboard-opened menus). */
	void highlightFirst();
	void activate(size_t index);
	/** Opens the submenu of an item; returns the child panel or null. */
	UIMenuPanel* openSubmenu(size_t index, bool fromKeyboard);
	UIMenuPanel* getOpenSubmenu() const { return m_child; }
	/** Row under a local y coordinate, or kNone. */
	size_t rowAt(float localY) const;
	/** Local rect of a row. */
	UIRect rowRect(size_t index) const;

	bool onMouseDown(const UIMouseEvent& event) override { return true; }
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	bool tick(float delta) override;
	void onPopupClosed(UIObject* popup) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	float itemHeight(const UIMenuItem& item) const;
	size_t nextSelectable(size_t from, int direction) const;
	void setHighlighted(size_t index);

	UIMenuItems m_root;
	std::vector<UIMenuItem>* m_items;
	UIMenuBar* m_bar;
	bool m_isSubmenu;
	size_t m_highlighted = kNone;
	size_t m_pendingSubmenu = kNone;
	float m_pendingTime = 0.0f;
	UIMenuPanel* m_child = nullptr;
	size_t m_childIndex = kNone;
};

}  // namespace bird

#endif	// !BIRD_UIMENUPANEL_H
