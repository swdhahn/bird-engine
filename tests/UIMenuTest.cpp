#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UIMenuBar.h"
#include "scene/2D/UIMenuPanel.h"
#include "scene/2D/UIPanel.h"

using namespace bird;
using bird::test::UIHarness;

namespace {

/** Centre of a menu row, in screen space. */
Vector2 rowCenter(UIHarness& h, UIMenuPanel* panel, size_t row) {
	const UIRect r = panel->rowRect(row);
	return h.atLocal(panel, r.position + r.size * 0.5f);
}

UIMenuPanel* topPanel(UIHarness& h) {
	return dynamic_cast<UIMenuPanel*>(h.ui.getTopPopup());
}

}  // namespace

TEST(UIContextMenu, RightClickOpensAndActivationCloses) {
	UIHarness h;
	int saves = 0;
	auto* target = h.root()->add<UIPanel>();
	target->setSize({200, 200});
	target->setContextMenu({UIMenuItem::action("Save", [&] { saves++; }, "Ctrl+S"),
							UIMenuItem::action("Close", [] {})});
	h.rightClick(h.at(target));
	UIMenuPanel* panel = topPanel(h);
	ASSERT_NE(panel, nullptr);

	h.click(rowCenter(h, panel, 0));
	EXPECT_EQ(saves, 1);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
}

TEST(UIContextMenu, KeyboardSkipsSeparatorsAndDisabled) {
	UIHarness h;
	auto items = makeMenu({UIMenuItem::action("A", [] {}), UIMenuItem::separator(),
						   UIMenuItem::action("B", [] {}).disabled(),
						   UIMenuItem::action("C", [] {})});
	h.ui.showContextMenu(items, Vector2(10, 10));
	UIMenuPanel* panel = topPanel(h);
	ASSERT_NE(panel, nullptr);
	h.press(UIKey::Down);
	EXPECT_EQ(panel->getHighlighted(), 0u);
	h.press(UIKey::Down);
	EXPECT_EQ(panel->getHighlighted(), 3u);
	h.press(UIKey::Down);  // wraps
	EXPECT_EQ(panel->getHighlighted(), 0u);
	h.press(UIKey::Up);
	EXPECT_EQ(panel->getHighlighted(), 3u);
}

TEST(UIContextMenu, CheckableItemsPersistTheirState) {
	UIHarness h;
	auto* target = h.root()->add<UIPanel>();
	target->setSize({200, 200});
	target->setContextMenu({UIMenuItem::toggle("Show grid", false)});
	h.rightClick(h.at(target));
	h.press(UIKey::Down);
	h.press(UIKey::Enter);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
	EXPECT_TRUE(target->getContextMenu()->at(0).checked);
}

TEST(UISubmenu, OpensAfterHoverDelay) {
	UIHarness h;
	auto items = makeMenu({UIMenuItem::submenu(
		"Recent", {UIMenuItem::action("one.txt", [] {})})});
	h.ui.showContextMenu(items, Vector2(10, 10));
	UIMenuPanel* panel = topPanel(h);
	h.ui.mouseMove(rowCenter(h, panel, 0));
	h.frame(0.1f);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);
	h.frame(0.1f);	// past the 0.15s delay
	EXPECT_EQ(h.ui.getPopupCount(), 2u);
	EXPECT_EQ(panel->getOpenSubmenu(), h.ui.getTopPopup());
}

TEST(UISubmenu, RightOpensLeftClosesEscapeClosesOneLevel) {
	UIHarness h;
	bool opened = false;
	auto items = makeMenu(
		{UIMenuItem::submenu("Recent", {UIMenuItem::action("one.txt",
															 [&] { opened = true; })}),
		 UIMenuItem::action("Quit", [] {})});
	h.ui.showContextMenu(items, Vector2(10, 10));
	h.press(UIKey::Down);
	h.press(UIKey::Right);
	ASSERT_EQ(h.ui.getPopupCount(), 2u);
	EXPECT_EQ(topPanel(h)->getHighlighted(), 0u);
	h.press(UIKey::Left);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);
	h.press(UIKey::Right);
	h.press(UIKey::Escape);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);
	h.press(UIKey::Right);
	h.press(UIKey::Enter);
	EXPECT_TRUE(opened);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);  // activation closes the chain
}

TEST(UIMenuBar, ClickOpensHoverSwitchesKeysMove) {
	UIHarness h;
	auto* bar = h.root()->add<UIMenuBar>();
	bar->addMenu("File", {UIMenuItem::action("New", [] {})});
	bar->addMenu("View", {UIMenuItem::action("Zoom", [] {})});
	bar->addMenu("Help", {UIMenuItem::action("About", [] {})});
	h.ui.layoutNow();
	auto titleCenter = [&](size_t i) {
		const UIRect r = bar->titleRect(i);
		return h.atLocal(bar, r.position + r.size * 0.5f);
	};

	h.click(titleCenter(0));
	EXPECT_EQ(bar->getOpenMenu(), 0u);
	h.ui.mouseMove(titleCenter(1));
	EXPECT_EQ(bar->getOpenMenu(), 1u);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);

	h.press(UIKey::Right);
	EXPECT_EQ(bar->getOpenMenu(), 2u);
	h.press(UIKey::Right);	// wraps
	EXPECT_EQ(bar->getOpenMenu(), 0u);
	h.press(UIKey::Escape);
	EXPECT_EQ(bar->getOpenMenu(), UIMenuBar::kNone);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
}
