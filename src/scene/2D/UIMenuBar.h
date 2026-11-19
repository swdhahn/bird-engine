#ifndef BIRD_UIMENUBAR_H
#define BIRD_UIMENUBAR_H

#include <string>
#include <vector>

#include "UIMenu.h"
#include "UIObject.h"

namespace bird {

class UIMenuPanel;

/**
 * Horizontal strip of menu titles. Clicking a title opens its menu below
 * it; while a menu is open, hovering another title switches to it, and
 * Left/Right inside a menu move between menus. Usually stretched across
 * the top of the window with setAnchors(TopLeft, TopRight).
 */
class UIMenuBar : public UIObject {
   public:
	static constexpr size_t kNone = static_cast<size_t>(-1);

	UIMenuBar();

	/** Appends a menu; its items persist (checked states survive). */
	UIMenuBar& addMenu(std::string title, std::vector<UIMenuItem> items);
	size_t getMenuCount() const { return m_menus.size(); }
	/** Index of the open menu, or kNone. */
	size_t getOpenMenu() const { return m_open; }
	UIMenuPanel* getOpenPanel() const { return m_panel; }
	/** Opens a menu (closing any other popups first). */
	void openMenu(size_t index, bool fromKeyboard = false);
	/** Moves to the neighbouring menu, wrapping (keyboard navigation). */
	void switchMenu(int delta);
	/** Local rect of a title. */
	UIRect titleRect(size_t index) const;
	size_t titleAt(float localX) const;

	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	void onHover(bool hovered) override;
	void onPopupClosed(UIObject* popup) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	struct Menu {
		std::string title;
		UIMenuItems items;
	};
	float titleWidth(size_t index) const;

	std::vector<Menu> m_menus;
	size_t m_open = kNone;
	size_t m_hoveredTitle = kNone;
	UIMenuPanel* m_panel = nullptr;
};

}  // namespace bird

#endif	// !BIRD_UIMENUBAR_H
