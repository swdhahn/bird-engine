#include <gtest/gtest.h>

#include <stdexcept>

#include "UIHarness.h"
#include "scene/2D/UICheckBox.h"
#include "scene/2D/UIDialog.h"
#include "scene/2D/UILabel.h"
#include "scene/2D/UIProgressBar.h"
#include "scene/2D/UISlider.h"
#include "scene/2D/UITabView.h"
#include "scene/2D/UIWindow.h"

using namespace bird;
using bird::test::UIHarness;

TEST(UICheckBox, ClickAndSpaceToggle) {
	UIHarness h;
	auto* check = h.root()->add<UICheckBox>("c");
	std::vector<bool> changes;
	check->setOnChanged([&](bool on) { changes.push_back(on); });
	h.click(check);
	EXPECT_TRUE(check->isChecked());
	h.press(UIKey::Space);	// focused by the click
	EXPECT_FALSE(check->isChecked());
	EXPECT_EQ(changes, (std::vector<bool>{true, false}));
}

TEST(UISlider, SnapsClampsAndSteps) {
	UISlider slider(0.0f, 1.0f, 0.0f);
	slider.setStep(0.25f);
	slider.setValue(0.6f);
	EXPECT_FLOAT_EQ(slider.getValue(), 0.5f);
	slider.setValue(5.0f);
	EXPECT_FLOAT_EQ(slider.getValue(), 1.0f);
	slider.onKey({UIKey::Left});
	EXPECT_FLOAT_EQ(slider.getValue(), 0.75f);
	slider.onKey({UIKey::Home});
	EXPECT_FLOAT_EQ(slider.getValue(), 0.0f);
	slider.onKey({UIKey::End});
	EXPECT_FLOAT_EQ(slider.getValue(), 1.0f);
}

TEST(UISlider, RejectsEmptyRange) {
	EXPECT_THROW(UISlider(1.0f, 1.0f), std::invalid_argument);
	UISlider slider;
	EXPECT_THROW(slider.setRange(2.0f, 1.0f), std::invalid_argument);
}

TEST(UISlider, NotifiesOnlyOnChange) {
	UISlider slider(0.0f, 10.0f, 5.0f);
	int calls = 0;
	slider.setOnChanged([&](float) { calls++; });
	slider.setValue(5.0f);
	slider.setValue(6.0f);
	EXPECT_EQ(calls, 1);
}

TEST(UIProgressBar, ClampsToUnitRange) {
	UIProgressBar bar;
	bar.setValue(1.7f);
	EXPECT_FLOAT_EQ(bar.getValue(), 1.0f);
	bar.setValue(-2.0f);
	EXPECT_FLOAT_EQ(bar.getValue(), 0.0f);
}

TEST(UITabView, ShowsOnlyTheSelectedPage) {
	UIHarness h;
	auto* tabs = h.root()->add<UITabView>();
	tabs->setSize({400, 300});
	UIObject* pages[3] = {tabs->addTab<UIObject>("A"), tabs->addTab<UIObject>("B"),
						  tabs->addTab<UIObject>("C")};
	EXPECT_TRUE(pages[0]->isVisible());
	EXPECT_FALSE(pages[1]->isVisible());

	size_t notified = 99;
	tabs->setOnTabChanged([&](size_t index) { notified = index; });
	h.click(tabs->getTabButton(2));
	EXPECT_EQ(tabs->getSelectedTab(), 2u);
	EXPECT_EQ(notified, 2u);
	EXPECT_FALSE(pages[0]->isVisible());
	EXPECT_TRUE(pages[2]->isVisible());
	EXPECT_TRUE(tabs->getTabButton(2)->isSelected());
	EXPECT_THROW(tabs->selectTab(3), std::out_of_range);
}

TEST(UITabView, PagesFillThePageArea) {
	UIHarness h;
	auto* tabs = h.root()->add<UITabView>();
	tabs->setSize({400, 300});
	UIObject* page = tabs->addTab<UIObject>("A");
	h.ui.layoutNow();
	// Page area = 300 - tab strip height (28), minus 8px padding each side.
	EXPECT_FLOAT_EQ(page->getSize().x, 400 - 16);
	EXPECT_FLOAT_EQ(page->getSize().y, 300 - 28 - 16);
}

TEST(UIWindow, DragsByTitleBarAndResizesWithLimit) {
	UIHarness h;
	auto* window = h.root()->add<UIWindow>("W", Vector2(300, 200));
	window->setOffset({100, 100});
	window->setMinSize({160, 100});
	h.ui.layoutNow();

	h.ui.mouseButton(UIMouseButton::Left, true, Vector2(110, 110));	// title
	h.ui.mouseMove(Vector2(160, 140));
	h.ui.mouseButton(UIMouseButton::Left, false, Vector2(160, 140));
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(window->getLayoutPosition().x, 150);
	EXPECT_FLOAT_EQ(window->getLayoutPosition().y, 130);

	const Vector2 grip = h.at(window, {1.0f, 1.0f}) - Vector2(4.0f);
	h.ui.mouseButton(UIMouseButton::Left, true, grip);
	h.ui.mouseMove(grip - Vector2(500.0f));
	h.ui.mouseButton(UIMouseButton::Left, false, grip - Vector2(500.0f));
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(window->getSize().x, 160);
	EXPECT_FLOAT_EQ(window->getSize().y, 100);
}

TEST(UIWindow, PressingBringsToFront) {
	UIHarness h;
	auto* back = h.root()->add<UIWindow>("back", Vector2(200, 200));
	auto* front = h.root()->add<UIWindow>("front", Vector2(200, 200));
	front->setOffset({300, 300});
	h.click(h.at(back, {0.5f, 0.9f}));
	EXPECT_EQ(h.root()->getChildren().back().get(), back);
	(void)front;
}

TEST(UIDialog, ButtonRunsCallbackThenCloses) {
	UIHarness h;
	bool confirmed = false;
	auto dialog = std::make_unique<UIDialog>("Title", "Message");
	dialog->addButton("OK", [&] { confirmed = true; });
	h.ui.pushModal(std::move(dialog));
	h.ui.layoutNow();
	// The OK button is focused when the modal opens.
	ASSERT_NE(h.ui.getFocus(), nullptr);
	h.click(h.ui.getFocus());
	h.frame();
	EXPECT_TRUE(confirmed);
	EXPECT_EQ(h.ui.getTopModal(), nullptr);
}

TEST(UILabel, MeasuresWithTheUiFont) {
	UIHarness h;
	auto* label = h.root()->add<UILabel>("abcd");
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(label->getSize().x, 28);
	EXPECT_FLOAT_EQ(label->getSize().y, 14);
	label->setFontSize(28.0f);
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(label->getSize().x, 56);
	EXPECT_THROW(label->setFontSize(0.0f), std::invalid_argument);
}
