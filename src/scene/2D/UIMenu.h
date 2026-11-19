#ifndef BIRD_UIMENU_H
#define BIRD_UIMENU_H

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace bird {

/**
 * One entry of a context menu, menu bar menu or submenu. Plain data, so
 * menus can be built declaratively:
 *
 *   { UIMenuItem::action("Save", save, "Ctrl+S"),
 *     UIMenuItem::separator(),
 *     UIMenuItem::submenu("Recent", {...}) }
 */
struct UIMenuItem {
	std::string text;
	std::string shortcut;  // display only, e.g. "Ctrl+S"
	std::function<void()> onActivate;
	bool enabled = true;
	bool checkable = false;
	bool checked = false;  // toggled on activation when checkable
	bool isSeparator = false;
	std::vector<UIMenuItem> children;  // non-empty: opens a submenu

	static UIMenuItem action(std::string text, std::function<void()> onActivate,
							 std::string shortcut = "") {
		UIMenuItem item;
		item.text = std::move(text);
		item.onActivate = std::move(onActivate);
		item.shortcut = std::move(shortcut);
		return item;
	}
	static UIMenuItem toggle(std::string text, bool checked,
							 std::function<void()> onActivate = {}) {
		UIMenuItem item = action(std::move(text), std::move(onActivate));
		item.checkable = true;
		item.checked = checked;
		return item;
	}
	static UIMenuItem submenu(std::string text,
							  std::vector<UIMenuItem> children) {
		UIMenuItem item;
		item.text = std::move(text);
		item.children = std::move(children);
		return item;
	}
	static UIMenuItem separator() {
		UIMenuItem item;
		item.isSeparator = true;
		item.enabled = false;
		return item;
	}
	UIMenuItem& disabled() {
		enabled = false;
		return *this;
	}
	bool isSelectable() const { return enabled && !isSeparator; }
};

/** Shared, mutable menu contents (checked states persist across opens). */
using UIMenuItems = std::shared_ptr<std::vector<UIMenuItem>>;

inline UIMenuItems makeMenu(std::vector<UIMenuItem> items) {
	return std::make_shared<std::vector<UIMenuItem>>(std::move(items));
}

}  // namespace bird

#endif	// !BIRD_UIMENU_H
