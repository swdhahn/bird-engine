#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UIButton.h"
#include "scene/2D/UIDialog.h"
#include "scene/2D/UIDropdown.h"
#include "scene/2D/UIPanel.h"
#include "scene/2D/UIScrollView.h"
#include "scene/2D/UIStack.h"

using namespace bird;
using bird::test::UIHarness;

namespace {

std::unique_ptr<UIPanel> box(Vector2 size) {
	auto panel = std::make_unique<UIPanel>();
	panel->setSize(size);
	return panel;
}

}  // namespace

TEST(UIPopup, OpensBelowAnchor) {
	UIHarness h;
	UIObject* popup = h.ui.openPopup(nullptr, box({50, 40}), {{100, 100}, {80, 28}});
	h.ui.layoutNow();
	EXPECT_EQ(popup->getLayoutPosition(), Vector2(100, 128));
}

TEST(UIPopup, FlipsAboveAndClampsToViewport) {
	UIHarness h;  // 800 x 600
	UIObject* above = h.ui.openPopup(nullptr, box({50, 100}), {{780, 580}, {20, 20}});
	h.ui.layoutNow();
	EXPECT_EQ(above->getLayoutPosition(), Vector2(750, 480));
}

TEST(UIPopup, RightPlacementFlipsLeft) {
	UIHarness h;
	UIObject* popup = h.ui.openPopup(nullptr, box({150, 50}), {{700, 100}, {80, 26}},
									 UIPopupPlacement::Right);
	h.ui.layoutNow();
	EXPECT_EQ(popup->getLayoutPosition(), Vector2(550, 100));
}

TEST(UIPopup, PressOutsideClosesAndIsConsumed) {
	UIHarness h;
	int clicks = 0;
	auto* button = h.root()->add<UIButton>("b");
	button->setOffset({300, 300});
	button->setOnClick([&] { clicks++; });
	h.ui.openPopup(nullptr, box({50, 40}), {{0, 0}, {10, 10}});
	EXPECT_TRUE(h.ui.wantsMouse());
	EXPECT_TRUE(h.ui.wantsKeyboard());

	h.click(button);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
	EXPECT_EQ(clicks, 0);  // the dismissing press did not click
	h.click(button);
	EXPECT_EQ(clicks, 1);
}

TEST(UIPopup, PopupsSitAboveModals) {
	UIHarness h;
	h.ui.pushModal(std::make_unique<UIDialog>("t", "m"));
	UIObject* popup = h.ui.openPopup(nullptr, box({50, 40}), {{10, 10}, {10, 10}});
	EXPECT_EQ(h.ui.hitTest(h.at(popup)), popup);
}

TEST(UIPopup, EscapeClosesOnlyTheTopPopup) {
	UIHarness h;
	UIObject* first = h.ui.openPopup(nullptr, box({50, 40}), {{10, 10}, {10, 10}});
	h.ui.openPopup(first, box({50, 40}), {{60, 10}, {10, 10}});
	h.press(UIKey::Escape);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);
	EXPECT_EQ(h.ui.getTopPopup(), first);
}

TEST(UIPopup, ClosingAPopupClosesLaterOnes) {
	UIHarness h;
	UIObject* first = h.ui.openPopup(nullptr, box({50, 40}), {{10, 10}, {10, 10}});
	h.ui.openPopup(first, box({50, 40}), {{60, 10}, {10, 10}});
	h.ui.closePopup(first);
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
}

TEST(UIPopup, RemovingTheOwnerClosesItsPopup) {
	UIHarness h;
	auto* owner = h.root()->add<UIButton>("owner");
	h.ui.openPopup(owner, box({50, 40}), {{10, 10}, {10, 10}});
	owner->removeFromParent();
	h.frame();
	EXPECT_EQ(h.ui.getPopupCount(), 0u);
	EXPECT_TRUE(h.ui.getOverlay()->getChildren().empty());
}

// ------------------------------------------------------------- dropdown

namespace {
const std::vector<std::string> kFruit = {"Apple", "Banana", "Cherry"};

Vector2 rowCenter(UIHarness& h, UIDropdownList* list, size_t row) {
	const float rowH = 26.0f;  // theme rowHeight
	return h.atLocal(list, {list->getSize().x * 0.5f,
							4.0f + rowH * static_cast<float>(row) + rowH * 0.5f});
}
}  // namespace

TEST(UIDropdown, ClickOpensAndRowClickSelects) {
	UIHarness h;
	auto* dropdown = h.root()->add<UIDropdown>(kFruit);
	size_t changed = UIDropdown::kNone;
	dropdown->setOnChanged([&](size_t i) { changed = i; });
	h.click(dropdown);
	ASSERT_TRUE(dropdown->isOpen());
	EXPECT_EQ(h.ui.getFocus(), dropdown);

	h.click(rowCenter(h, dropdown->getList(), 2));
	EXPECT_FALSE(dropdown->isOpen());
	EXPECT_EQ(dropdown->getSelectedIndex(), 2u);
	EXPECT_EQ(dropdown->getSelectedText(), "Cherry");
	EXPECT_EQ(changed, 2u);
}

TEST(UIDropdown, ClickingTheDropdownAgainCloses) {
	UIHarness h;
	auto* dropdown = h.root()->add<UIDropdown>(kFruit);
	h.click(dropdown);
	ASSERT_TRUE(dropdown->isOpen());
	h.click(dropdown);
	EXPECT_FALSE(dropdown->isOpen());
}

TEST(UIDropdown, KeyboardWhileClosedAndOpen) {
	UIHarness h;
	auto* dropdown = h.root()->add<UIDropdown>(kFruit);
	h.ui.setFocus(dropdown);
	h.press(UIKey::Down);
	EXPECT_EQ(dropdown->getSelectedIndex(), 0u);
	h.press(UIKey::Down);
	EXPECT_EQ(dropdown->getSelectedIndex(), 1u);

	h.press(UIKey::Enter);	// open
	ASSERT_TRUE(dropdown->isOpen());
	EXPECT_EQ(dropdown->getList()->getHighlighted(), 1u);
	h.press(UIKey::Down);
	h.press(UIKey::Enter);	// choose Cherry
	EXPECT_FALSE(dropdown->isOpen());
	EXPECT_EQ(dropdown->getSelectedIndex(), 2u);

	h.press(UIKey::Enter);
	h.press(UIKey::Escape);
	EXPECT_FALSE(dropdown->isOpen());
	EXPECT_EQ(dropdown->getSelectedIndex(), 2u);  // Escape keeps selection
}

TEST(UIDropdown, TypeAheadSelectsByInitial) {
	UIHarness h;
	auto* dropdown = h.root()->add<UIDropdown>(
		std::vector<std::string>{"Apple", "Banana", "blueberry", "Cherry"});
	h.ui.setFocus(dropdown);
	h.type("b");
	EXPECT_EQ(dropdown->getSelectedIndex(), 1u);
	h.type("b");
	EXPECT_EQ(dropdown->getSelectedIndex(), 2u);  // next match
	h.type("c");
	EXPECT_EQ(dropdown->getSelectedIndex(), 3u);
}

TEST(UIDropdown, LongListScrollsToSelection) {
	UIHarness h;
	std::vector<std::string> items;
	for (int i = 0; i < 20; i++) items.push_back("Item " + std::to_string(i));
	auto* dropdown = h.root()->add<UIDropdown>(items);
	dropdown->setSelectedIndex(15);
	h.click(dropdown);
	h.ui.layoutNow();
	UIDropdownList* list = dropdown->getList();
	ASSERT_NE(list, nullptr);
	// 8 visible rows of 26px between 4px paddings.
	EXPECT_FLOAT_EQ(list->getSize().y, 8 * 26 + 8);
	EXPECT_GT(list->getScrollOffset(), 0.0f);
	h.press(UIKey::End);
	EXPECT_EQ(list->getHighlighted(), 19u);
	EXPECT_FLOAT_EQ(list->getScrollOffset(), 12 * 26);	// max scroll
}

TEST(UIDropdown, ListIsNotClippedByScrollView) {
	UIHarness h;
	auto* scroll = h.root()->add<UIScrollView>();
	scroll->setSize({200, 40});
	auto* column = scroll->setContent<UIStack>(UIAxis::Vertical);
	auto* dropdown = column->add<UIDropdown>(kFruit);
	h.click(dropdown);
	UIDropdownList* list = dropdown->getList();
	ASSERT_NE(list, nullptr);
	const Vector2 lastRow = rowCenter(h, list, 2);
	EXPECT_GT(lastRow.y, 40.0f);  // outside the scroll view
	EXPECT_EQ(h.ui.hitTest(lastRow), list);
}
