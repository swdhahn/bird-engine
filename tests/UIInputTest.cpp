#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UIButton.h"
#include "scene/2D/UICheckBox.h"
#include "scene/2D/UIDialog.h"
#include "scene/2D/UIScrollView.h"
#include "scene/2D/UISlider.h"
#include "scene/2D/UIStack.h"
#include "scene/2D/UITextBox.h"
#include "scene/2D/UITooltip.h"
#include "scene/2D/UIWindow.h"

using namespace bird;
using bird::test::UIHarness;

TEST(UIInput, ClickInsideFiresButtonReleaseOutsideDoesNot) {
	UIHarness h;
	int clicks = 0;
	auto* button = h.root()->add<UIButton>("Go");
	button->setOnClick([&] { clicks++; });
	h.click(button);
	EXPECT_EQ(clicks, 1);

	h.ui.mouseButton(UIMouseButton::Left, true, h.at(button));
	h.ui.mouseButton(UIMouseButton::Left, false, Vector2(700, 500));
	EXPECT_EQ(clicks, 1);
	EXPECT_FALSE(button->isPressed());
}

TEST(UIInput, DisabledButtonIgnoresClicks) {
	UIHarness h;
	int clicks = 0;
	auto* button = h.root()->add<UIButton>("Go");
	button->setOnClick([&] { clicks++; });
	button->setEnabled(false);
	h.click(button);
	EXPECT_EQ(clicks, 0);
}

TEST(UIInput, CaptureKeepsDragAliveOutsideTheWidget) {
	UIHarness h;
	auto* slider = h.root()->add<UISlider>(0.0f, 1.0f, 0.5f);
	slider->setSize({200, 28});
	h.ui.mouseButton(UIMouseButton::Left, true, h.at(slider, {0.0f, 0.5f}));
	EXPECT_FLOAT_EQ(slider->getValue(), 0.0f);
	EXPECT_EQ(h.ui.getCaptured(), slider);
	h.ui.mouseMove(Vector2(5000, 5000));  // far outside
	EXPECT_FLOAT_EQ(slider->getValue(), 1.0f);
	h.ui.mouseButton(UIMouseButton::Left, false, Vector2(5000, 5000));
	EXPECT_EQ(h.ui.getCaptured(), nullptr);
}

TEST(UIInput, ScrollBubblesFromButtonToScrollView) {
	UIHarness h;
	auto* scroll = h.root()->add<UIScrollView>();
	scroll->setSize({200, 100});
	auto* list = scroll->setContent<UIStack>(UIAxis::Vertical);
	std::vector<UIButton*> buttons;
	for (int i = 0; i < 10; i++) buttons.push_back(list->add<UIButton>("b"));
	h.ui.layoutNow();
	// 10 x 28px buttons + 9 x 6px gaps = 334px of content in 100px.
	EXPECT_FLOAT_EQ(scroll->getMaxScroll(), 234.0f);

	h.ui.scroll(h.at(buttons[0]), Vector2(0.0f, -1.0f));
	EXPECT_FLOAT_EQ(scroll->getScrollOffset(), UIScrollView::kWheelStep);
}

TEST(UIInput, ClippedContentIsNotHittable) {
	UIHarness h;
	auto* scroll = h.root()->add<UIScrollView>();
	scroll->setSize({200, 100});
	auto* list = scroll->setContent<UIStack>(UIAxis::Vertical);
	std::vector<UIButton*> buttons;
	for (int i = 0; i < 10; i++) buttons.push_back(list->add<UIButton>("b"));
	h.ui.layoutNow();
	EXPECT_NE(h.ui.hitTest(h.at(buttons[8])), buttons[8]);
	scroll->setScrollOffset(scroll->getMaxScroll());
	EXPECT_EQ(h.ui.hitTest(h.at(buttons[9])), buttons[9]);
}

TEST(UIInput, TabCyclesFocusSkippingDisabled) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	auto* text = stack->add<UITextBox>();
	auto* button = stack->add<UIButton>("b");
	auto* check = stack->add<UICheckBox>("c");
	stack->add<UIButton>("off")->setEnabled(false);
	auto* slider = stack->add<UISlider>();

	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), text);
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), button);
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), check);
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), slider);
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), text);  // wraps
	h.press(UIKey::Tab, UI_MOD_SHIFT);
	EXPECT_EQ(h.ui.getFocus(), slider);
}

TEST(UIInput, ClickingEmptySpaceClearsFocus) {
	UIHarness h;
	auto* text = h.root()->add<UITextBox>();
	h.click(text);
	EXPECT_EQ(h.ui.getFocus(), text);
	h.click(Vector2(700, 500));
	EXPECT_EQ(h.ui.getFocus(), nullptr);
}

TEST(UIInput, ModalBlocksInputBelowAndEscapeCloses) {
	UIHarness h;
	int clicks = 0;
	auto* button = h.root()->add<UIButton>("Go");
	button->setOnClick([&] { clicks++; });
	const Vector2 buttonCenter = h.at(button);

	h.ui.pushModal(std::make_unique<UIDialog>("Title", "Message"));
	EXPECT_TRUE(h.ui.wantsMouse());
	h.click(buttonCenter);
	EXPECT_EQ(clicks, 0);

	h.press(UIKey::Escape);
	h.frame();
	EXPECT_EQ(h.ui.getTopModal(), nullptr);
	EXPECT_TRUE(h.ui.getOverlay()->getChildren().empty());
	h.click(buttonCenter);
	EXPECT_EQ(clicks, 1);
}

TEST(UIInput, ClosingAWindowFromItsOwnButtonIsSafe) {
	UIHarness h;
	auto* window = h.root()->add<UIWindow>("W", Vector2(300, 200));
	UIObject* closeButton = window->getChildren()[1].get();
	h.click(closeButton);  // destroys the window during event handling
	EXPECT_TRUE(h.root()->getChildren().empty());
	EXPECT_EQ(h.ui.getHovered(), nullptr);
	EXPECT_EQ(h.ui.getCaptured(), nullptr);
	h.frame();	// draws without touching freed nodes
}

TEST(UIInput, KeyboardIsWantedOnlyByTextInput) {
	UIHarness h;
	auto* text = h.root()->add<UITextBox>();
	auto* button = h.root()->add<UIButton>("b");
	button->setOffset({0, 100});
	h.click(text);
	EXPECT_TRUE(h.ui.wantsKeyboard());
	h.click(button);
	EXPECT_FALSE(h.ui.wantsKeyboard());
}

TEST(UIInput, MouseIsWantedOverInteractiveUiOnly) {
	UIHarness h;
	auto* button = h.root()->add<UIButton>("b");
	h.ui.mouseMove(h.at(button));
	EXPECT_TRUE(h.ui.wantsMouse());
	h.ui.mouseMove(Vector2(700, 500));
	EXPECT_FALSE(h.ui.wantsMouse());
}

TEST(UIInput, TooltipAppearsAfterDelayAndHidesOnLeave) {
	UIHarness h;
	auto* button = h.root()->add<UIButton>("b");
	button->setTooltip("Helpful");
	h.ui.mouseMove(h.at(button));
	h.frame(0.2f);
	EXPECT_TRUE(h.ui.getOverlay()->getChildren().empty());
	h.frame(0.4f);
	ASSERT_EQ(h.ui.getOverlay()->getChildren().size(), 1u);
	auto* tip = dynamic_cast<UITooltip*>(h.ui.getOverlay()->getChildren()[0].get());
	ASSERT_NE(tip, nullptr);
	EXPECT_EQ(tip->getText(), "Helpful");

	h.ui.mouseMove(Vector2(700, 500));
	h.frame();
	EXPECT_TRUE(h.ui.getOverlay()->getChildren().empty());
}

TEST(UIInput, HoverSetsCursorShape) {
	UIHarness h;
	auto* text = h.root()->add<UITextBox>();
	h.ui.mouseMove(h.at(text));
	EXPECT_EQ(h.platform.cursor, UICursor::IBeam);
	h.ui.mouseMove(Vector2(700, 500));
	EXPECT_EQ(h.platform.cursor, UICursor::Arrow);
}
