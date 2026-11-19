#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UIColor.h"
#include "scene/2D/UIColorControls.h"
#include "scene/2D/UIColorPicker.h"
#include "scene/2D/UINumberField.h"
#include "scene/2D/UITextBox.h"

using namespace bird;
using bird::test::UIHarness;
using Channel = UIColorPicker::Channel;

namespace {

void expectColor(const Vector4& actual, const Vector4& expected, float tol = 0.004f) {
	for (int i = 0; i < 4; i++) EXPECT_NEAR(actual[i], expected[i], tol) << "channel " << i;
}

}  // namespace

// ------------------------------------------------------------ conversion

TEST(UIColorMath, HsvToRgbPrimariesAndMixes) {
	expectColor(Vector4(color::hsvToRgb({0, 1, 1}), 1), {1, 0, 0, 1});
	expectColor(Vector4(color::hsvToRgb({120, 1, 1}), 1), {0, 1, 0, 1});
	expectColor(Vector4(color::hsvToRgb({240, 1, 1}), 1), {0, 0, 1, 1});
	expectColor(Vector4(color::hsvToRgb({60, 0.5f, 1}), 1), {1, 1, 0.5f, 1});
	expectColor(Vector4(color::hsvToRgb({200, 0, 0.5f}), 1), {0.5f, 0.5f, 0.5f, 1});
	expectColor(Vector4(color::hsvToRgb({-120, 1, 1}), 1), {0, 0, 1, 1});	// wraps
}

TEST(UIColorMath, RgbToHsvAndGreyKeepsFallbackHue) {
	const color::HSV orange = color::rgbToHsv({1, 0.5f, 0});
	EXPECT_NEAR(orange.h, 30, 0.01);
	EXPECT_NEAR(orange.s, 1, 1e-6);
	EXPECT_NEAR(orange.v, 1, 1e-6);
	const color::HSV grey = color::rgbToHsv({0.4f, 0.4f, 0.4f}, 210.0f);
	EXPECT_FLOAT_EQ(grey.h, 210.0f);
	EXPECT_FLOAT_EQ(grey.s, 0.0f);
	for (float h = 0; h < 360; h += 37) {
		const color::HSV back = color::rgbToHsv(color::hsvToRgb({h, 0.7f, 0.6f}));
		EXPECT_NEAR(back.h, h, 0.05);
		EXPECT_NEAR(back.s, 0.7f, 1e-4);
		EXPECT_NEAR(back.v, 0.6f, 1e-4);
	}
}

TEST(UIColorMath, HexFormatting) {
	EXPECT_EQ(color::toHex({1, 0.5f, 0, 1}), "#FF8000");
	EXPECT_EQ(color::toHex({1, 0.5f, 0, 0.5f}), "#FF800080");
	EXPECT_EQ(color::toHex({0, 0, 0, 1}, true), "#000000FF");
	EXPECT_EQ(color::toHex({2, -1, 0.5f, 1}), "#FF0080");	// clamped
}

TEST(UIColorMath, HexParsingForms) {
	expectColor(*color::parseHex("#F80"), {1, 0x88 / 255.0f, 0, 1});
	expectColor(*color::parseHex("f80c"), {1, 0x88 / 255.0f, 0, 0xCC / 255.0f});
	expectColor(*color::parseHex("#FF8000"), {1, 128 / 255.0f, 0, 1});
	expectColor(*color::parseHex("  #ff800080 "), {1, 128 / 255.0f, 0, 128 / 255.0f});
	for (const char* bad : {"", "#", "#GG0000", "#12345", "#1234567", "red", "##fff"}) {
		EXPECT_FALSE(color::parseHex(bad).has_value()) << bad;
	}
}

// ---------------------------------------------------------------- picker

TEST(UIColorPicker, SetColorSyncsEveryControl) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0.5f, 0, 1));
	EXPECT_EQ(picker->getHex(), "#FF8000");
	EXPECT_EQ(picker->getHexField()->getText(), "#FF8000");
	EXPECT_DOUBLE_EQ(picker->getField(Channel::R)->getValue(), 255.0);
	EXPECT_DOUBLE_EQ(picker->getField(Channel::G)->getValue(), 128.0);
	EXPECT_DOUBLE_EQ(picker->getField(Channel::H)->getValue(), 30.0);
	EXPECT_DOUBLE_EQ(picker->getField(Channel::V)->getValue(), 100.0);
	EXPECT_NEAR(picker->getWheel()->getHue(), 30.0f, 0.01f);
	EXPECT_FLOAT_EQ(picker->getValueSlider()->getValue(), 1.0f);
	EXPECT_NEAR(picker->getChannelSlider(Channel::G)->getValue(), 0.5f, 1e-4f);
}

TEST(UIColorPicker, TypingHexAppliesLiveAndNotifies) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0, 0, 1));
	Vector4 reported(-1.0f);
	picker->setOnChanged([&](const Vector4& c) { reported = c; });
	h.click(picker->getHexField());
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("#00ff00");
	expectColor(picker->getColor(), {0, 1, 0, 1});
	expectColor(reported, {0, 1, 0, 1});
	EXPECT_NEAR(picker->getWheel()->getHue(), 120.0f, 0.01f);
	EXPECT_DOUBLE_EQ(picker->getField(Channel::R)->getValue(), 0.0);
	h.press(UIKey::Enter);
	EXPECT_EQ(picker->getHexField()->getText(), "#00FF00");	 // normalized
}

TEST(UIColorPicker, InvalidHexIsRejected) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0, 0, 1));
	EXPECT_FALSE(picker->setHex("#XYZ"));
	EXPECT_EQ(picker->getHex(), "#FF0000");
	h.click(picker->getHexField());
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("#12");
	h.press(UIKey::Enter);
	EXPECT_EQ(picker->getHexField()->getText(), "#FF0000");	 // reverted
	EXPECT_NE(picker->getStatus().find("Not a hex"), std::string::npos);
	EXPECT_TRUE(picker->setHex("00f"));
	EXPECT_EQ(picker->getHex(), "#0000FF");
}

TEST(UIColorPicker, WheelClickSetsHueAndSaturation) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 1, 1, 1));
	UIColorWheel* wheel = picker->getWheel();
	// Diameter 180: centre (90, 90), radius 88.
	h.click(h.atLocal(wheel, {90 + 44, 90}));
	EXPECT_NEAR(picker->getHSV().h, 0.0f, 0.01f);
	EXPECT_NEAR(picker->getHSV().s, 0.5f, 0.01f);
	h.click(h.atLocal(wheel, {90, 90 - 88}));
	EXPECT_NEAR(picker->getHSV().h, 90.0f, 0.01f);
	EXPECT_NEAR(picker->getHSV().s, 1.0f, 0.01f);
	expectColor(picker->getColor(), Vector4(color::hsvToRgb({90, 1, 1}), 1));
}

TEST(UIColorPicker, ValueToBlackKeepsTheHue) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(0, 0, 1, 1));  // hue 240
	h.click(h.atLocal(picker->getValueSlider(), {9, 179.9f}));	 // bottom: v = 0
	EXPECT_EQ(picker->getHex(), "#000000");
	EXPECT_NEAR(picker->getWheel()->getHue(), 240.0f, 0.01f);
	h.ui.setFocus(picker->getField(Channel::V));
	h.press(UIKey::Up, UI_MOD_SHIFT);  // V 0 -> 10 %
	EXPECT_NEAR(picker->getHSV().h, 240.0f, 0.01f);
	EXPECT_EQ(picker->getHex(), "#00001A");
}

TEST(UIColorPicker, RgbSlidersAndFieldsEditOneChannel) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0, 0, 1));
	UIColorSlider* green = picker->getChannelSlider(Channel::G);
	h.ui.layoutNow();
	h.click(h.atLocal(green, {green->getSize().x * 0.5f, 9}));
	EXPECT_NEAR(picker->getColor().g, 0.5f, 0.01f);
	EXPECT_NEAR(picker->getColor().r, 1.0f, 1e-4f);

	UINumberField* blue = picker->getField(Channel::B);
	h.click(blue);
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("255");
	h.press(UIKey::Enter);
	EXPECT_NEAR(picker->getColor().b, 1.0f, 1e-4f);
	EXPECT_NEAR(green->getValue(), 0.5f, 0.01f);
}

TEST(UIColorPicker, AlphaFieldAndHexAlpha) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0, 0, 1));
	UINumberField* alpha = picker->getField(Channel::A);
	h.click(alpha);
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.type("50");
	h.press(UIKey::Enter);
	EXPECT_EQ(picker->getHex(), "#FF000080");
	EXPECT_NEAR(picker->getAlphaSlider()->getValue(), 0.5f, 1e-4f);
}

TEST(UIColorPicker, CopyPasteAndRevert) {
	UIHarness h;
	auto* picker = h.root()->add<UIColorPicker>(Vector4(1, 0.5f, 0, 1));
	picker->copyHex();
	EXPECT_EQ(h.platform.clipboard, "#FF8000");
	h.platform.clipboard = "#123456";
	EXPECT_TRUE(picker->pasteHex());
	EXPECT_EQ(picker->getHex(), "#123456");
	h.platform.clipboard = "hello";
	EXPECT_FALSE(picker->pasteHex());
	EXPECT_EQ(picker->getHex(), "#123456");

	// The swatch's left half is the colour from setColor: click reverts.
	UIColorSwatch* swatch = picker->getSwatch();
	h.click(h.atLocal(swatch, {5, 5}));
	EXPECT_EQ(picker->getHex(), "#FF8000");
}

TEST(UIColorPicker, SetColorDoesNotNotify) {
	UIColorPicker picker;
	int calls = 0;
	picker.setOnChanged([&](const Vector4&) { calls++; });
	picker.setColor(Vector4(0.2f, 0.3f, 0.4f, 1));
	EXPECT_EQ(calls, 0);
	picker.setHex("#ffffff");
	EXPECT_EQ(calls, 1);
}

TEST(UIColorButton, OpensPickerPopupAndFollowsIt) {
	UIHarness h;
	auto* button = h.root()->add<UIColorButton>(Vector4(1, 0, 0, 1));
	Vector4 reported(-1.0f);
	button->setOnColorChanged([&](const Vector4& c) { reported = c; });
	h.click(button);
	ASSERT_NE(button->getOpenPicker(), nullptr);
	EXPECT_EQ(h.ui.getPopupCount(), 1u);
	button->getOpenPicker()->setHex("#0000ff");
	expectColor(button->getColor(), {0, 0, 1, 1});
	expectColor(reported, {0, 0, 1, 1});
	h.click(Vector2(790, 590));	 // outside: light dismiss
	EXPECT_EQ(button->getOpenPicker(), nullptr);
}
