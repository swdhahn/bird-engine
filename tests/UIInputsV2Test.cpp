#include <gtest/gtest.h>

#include <stdexcept>

#include "UIHarness.h"
#include "scene/2D/UIButton.h"
#include "scene/2D/UINumberField.h"
#include "scene/2D/UIRadioGroup.h"
#include "scene/2D/UIStack.h"
#include "scene/2D/UITextBox.h"
#include "scene/2D/UIToggle.h"

using namespace bird;
using bird::test::UIHarness;

// --------------------------------------------------------------- toggle

TEST(UIToggle, ClickTogglesAndKnobAnimates) {
	UIHarness h;
	auto* toggle = h.root()->add<UIToggle>("V-Sync");
	std::vector<bool> changes;
	toggle->setOnChanged([&](bool on) { changes.push_back(on); });
	h.click(toggle);
	EXPECT_TRUE(toggle->isOn());
	EXPECT_FLOAT_EQ(toggle->getKnobPosition(), 0.0f);  // not yet animated
	h.frame(0.06f);
	EXPECT_NEAR(toggle->getKnobPosition(), 0.5f, 1e-4f);
	h.frame(0.1f);
	EXPECT_FLOAT_EQ(toggle->getKnobPosition(), 1.0f);
	h.press(UIKey::Space);
	EXPECT_FALSE(toggle->isOn());
	EXPECT_EQ(changes, (std::vector<bool>{true, false}));
}

TEST(UIToggle, DetachedToggleSnaps) {
	UIToggle toggle;
	toggle.setOn(true);
	EXPECT_FLOAT_EQ(toggle.getKnobPosition(), 1.0f);
}

// ---------------------------------------------------------------- radio

TEST(UIRadioGroup, ClickSelectsAndArrowsMoveWithFocus) {
	UIHarness h;
	auto* group = h.root()->add<UIRadioGroup>();
	auto* a = group->addOption("Windowed");
	auto* b = group->addOption("Borderless");
	auto* c = group->addOption("Fullscreen");
	size_t changed = UIRadioGroup::kNone;
	group->setOnChanged([&](size_t i) { changed = i; });

	h.click(c);
	EXPECT_EQ(group->getSelected(), 2u);
	EXPECT_EQ(changed, 2u);
	EXPECT_EQ(h.ui.getFocus(), c);
	EXPECT_TRUE(c->isSelected());

	h.press(UIKey::Down);  // wraps
	EXPECT_EQ(group->getSelected(), 0u);
	EXPECT_EQ(h.ui.getFocus(), a);
	h.press(UIKey::Up);
	EXPECT_EQ(h.ui.getFocus(), c);
	(void)b;
}

TEST(UIRadioGroup, GroupIsOneTabStop) {
	UIHarness h;
	auto* column = h.root()->add<UIStack>(UIAxis::Vertical);
	auto* before = column->add<UIButton>("before");
	auto* group = column->add<UIRadioGroup>();
	group->addOption("a");
	auto* second = group->addOption("b");
	group->addOption("c");
	auto* after = column->add<UIButton>("after");
	group->setSelected(1);

	h.ui.setFocus(before);
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), second);	 // only the selected option
	h.press(UIKey::Tab);
	EXPECT_EQ(h.ui.getFocus(), after);
}

TEST(UIRadioGroup, RejectsInvalidSelection) {
	UIRadioGroup group;
	group.addOption("a");
	EXPECT_THROW(group.setSelected(3), std::out_of_range);
	group.setSelected(UIRadioGroup::kNone);	// clearing is allowed
	EXPECT_EQ(group.getSelected(), UIRadioGroup::kNone);
}

// ------------------------------------------------------------- password

TEST(UITextBoxPassword, MasksTextAndBlocksCopy) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>();
	box->setPassword(true);
	h.click(box);
	h.type("hunter2");
	EXPECT_EQ(box->getText(), "hunter2");
	EXPECT_EQ(box->displayText(), "*******");
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.press(UIKey::C, UI_MOD_CONTROL);
	h.press(UIKey::X, UI_MOD_CONTROL);
	EXPECT_EQ(h.platform.clipboard, "");
	EXPECT_EQ(box->getText(), "hunter2");  // cut refused too
}

// --------------------------------------------------------- number field

TEST(UINumberField, StepsClampsAndFormats) {
	UIHarness h;
	auto* field = h.root()->add<UINumberField>(0.0, 10.0, 3.0);
	field->setStep(0.5).setPrecision(1);
	EXPECT_EQ(field->getText(), "3.0");
	h.ui.setFocus(field);
	h.press(UIKey::Up);
	EXPECT_DOUBLE_EQ(field->getValue(), 3.5);
	h.press(UIKey::Up, UI_MOD_SHIFT);
	EXPECT_DOUBLE_EQ(field->getValue(), 8.5);
	h.press(UIKey::Up, UI_MOD_SHIFT);
	EXPECT_DOUBLE_EQ(field->getValue(), 10.0);	// clamped
	EXPECT_EQ(field->getText(), "10.0");
}

TEST(UINumberField, TypingCommitsSnappedOrReverts) {
	UIHarness h;
	auto* field = h.root()->add<UINumberField>(0.0, 10.0, 3.0);
	field->setStep(0.5).setPrecision(1);
	double reported = -1.0;
	field->setOnValueChanged([&](double v) { reported = v; });
	h.click(field);
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("7.26xyz");	// letters other than e are rejected
	EXPECT_EQ(field->getText(), "7.26");
	h.press(UIKey::Enter);
	EXPECT_DOUBLE_EQ(field->getValue(), 7.5);
	EXPECT_DOUBLE_EQ(reported, 7.5);

	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("1-2");
	h.ui.setFocus(nullptr);	 // blur commits; invalid text reverts
	EXPECT_EQ(field->getText(), "7.5");
	EXPECT_DOUBLE_EQ(field->getValue(), 7.5);
}

TEST(UINumberField, WheelAndSteppersAndScrub) {
	UIHarness h;
	auto* field = h.root()->add<UINumberField>(0.0, 100.0, 50.0);
	field->setSize({120, 28});
	h.ui.layoutNow();
	const Vector2 center = h.at(field);
	h.ui.mouseMove(center);	 // hover enables the wheel
	h.ui.scroll(center, Vector2(0, 1));
	EXPECT_DOUBLE_EQ(field->getValue(), 51.0);

	// Bottom half of the stepper lane steps down; dragging up scrubs.
	const Vector2 stepper = h.atLocal(field, {120 - 9, 21});
	h.ui.mouseMove(stepper);
	h.ui.mouseButton(UIMouseButton::Left, true, stepper);
	EXPECT_DOUBLE_EQ(field->getValue(), 50.0);
	h.ui.mouseMove(stepper - Vector2(0, 18));  // 3 x 6px
	EXPECT_DOUBLE_EQ(field->getValue(), 53.0);
	h.ui.mouseButton(UIMouseButton::Left, false, stepper - Vector2(0, 18));
}

TEST(UINumberField, RejectsEmptyRange) {
	EXPECT_THROW(UINumberField(5.0, 5.0), std::invalid_argument);
}
