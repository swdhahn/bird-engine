#include <gtest/gtest.h>

#include <stdexcept>

#include "UIHarness.h"
#include "renders/generic/Texture.h"
#include "scene/2D/UIDrawList.h"
#include "scene/2D/UIRichInputs.h"

using namespace bird;
using bird::test::UIHarness;

// ---------------------------------------------------------- range slider

TEST(UIRangeSlider, OrdersClampsAndKeepsGap) {
	UIRangeSlider slider(0, 100, 80, 20);
	EXPECT_FLOAT_EQ(slider.getLow(), 20);
	EXPECT_FLOAT_EQ(slider.getHigh(), 80);
	slider.setMinGap(10);
	slider.setValues(50, 52);
	EXPECT_FLOAT_EQ(slider.getLow(), 50);
	EXPECT_FLOAT_EQ(slider.getHigh(), 60);
	EXPECT_THROW(UIRangeSlider(1, 1), std::invalid_argument);
}

TEST(UIRangeSlider, PressGrabsNearestThumbWhichCannotCross) {
	UIHarness h;
	auto* slider = h.root()->add<UIRangeSlider>(0.0f, 100.0f, 20.0f, 80.0f);
	slider->setSize({214, 28});	 // 200px of travel plus a 14px thumb
	std::vector<std::pair<float, float>> changes;
	slider->setOnChanged([&](float lo, float hi) { changes.emplace_back(lo, hi); });
	auto at = [&](float value) { return h.atLocal(slider, {7 + value * 2, 14}); };

	h.ui.mouseButton(UIMouseButton::Left, true, at(70));  // nearer the high thumb
	EXPECT_EQ(slider->getActiveThumb(), 1);
	EXPECT_FLOAT_EQ(slider->getHigh(), 70);
	h.ui.mouseMove(at(5));	// dragged past the low thumb: stops at it
	EXPECT_FLOAT_EQ(slider->getHigh(), 20);
	EXPECT_FLOAT_EQ(slider->getLow(), 20);
	h.ui.mouseButton(UIMouseButton::Left, false, at(5));
	EXPECT_FALSE(changes.empty());
}

TEST(UIRangeSlider, KeysMoveTheActiveThumbAndSpaceSwitches) {
	UIRangeSlider slider(0, 10, 2, 8);
	slider.setStep(1);
	slider.onKey({UIKey::Right});
	EXPECT_FLOAT_EQ(slider.getLow(), 3);
	slider.onKey({UIKey::Space});
	slider.onKey({UIKey::End});
	EXPECT_FLOAT_EQ(slider.getHigh(), 10);
	slider.onKey({UIKey::Home});  // stops at the low thumb
	EXPECT_FLOAT_EQ(slider.getHigh(), 3);
}

// ------------------------------------------------------- key bind field

TEST(UIKeyBindField, CapturesTheNextKey) {
	UIHarness h;
	auto* field = h.root()->add<UIKeyBindField>(87);  // W
	EXPECT_EQ(field->getDisplayText(), "Key 87");	   // fake platform name
	int changed = 0;
	field->setOnChanged([&](int key) { changed = key; });

	h.click(field);
	EXPECT_TRUE(field->isCapturing());
	EXPECT_TRUE(h.ui.wantsKeyboard());	// game actions are suppressed
	EXPECT_EQ(field->getDisplayText(), "Press a key...");
	h.ui.key(UIKey::Tab, true, false, UI_MOD_NONE, 258);  // even Tab binds
	EXPECT_FALSE(field->isCapturing());
	EXPECT_EQ(field->getKey(), 258);
	EXPECT_EQ(changed, 258);
	EXPECT_EQ(h.ui.getFocus(), field);	// Tab did not move focus
}

TEST(UIKeyBindField, EscapeCancelsBackspaceClearsBlurCancels) {
	UIHarness h;
	auto* field = h.root()->add<UIKeyBindField>(87);
	h.click(field);
	h.ui.key(UIKey::Escape, true, false, UI_MOD_NONE, 256);
	EXPECT_EQ(field->getKey(), 87);
	h.press(UIKey::Enter);	// restart capture from the keyboard
	EXPECT_TRUE(field->isCapturing());
	h.ui.key(UIKey::Backspace, true, false, UI_MOD_NONE, 259);
	EXPECT_EQ(field->getKey(), UIKeyBindField::kUnbound);
	EXPECT_EQ(field->getDisplayText(), "Unbound");
	h.click(field);
	h.click(Vector2(700, 500));	 // clicking elsewhere cancels
	EXPECT_FALSE(field->isCapturing());
	EXPECT_FALSE(h.ui.wantsKeyboard());
}

// ------------------------------------------------------------------ image

namespace {
std::shared_ptr<Texture> fakeTexture(uint32_t w, uint32_t h) {
	return std::make_shared<Texture>(nullptr, w, h, FORMAT_8RGBA);
}
}  // namespace

TEST(UIImage, FitModesPlaceAndCrop) {
	UIHarness h;
	auto* image = h.root()->add<UIImage>(fakeTexture(200, 100));
	image->setSize({100, 100});
	h.ui.layoutNow();

	image->setFit(UIImageFit::Contain);
	UIImage::Placement p = image->computePlacement();
	EXPECT_EQ(p.rect.position, Vector2(0, 25));
	EXPECT_EQ(p.rect.size, Vector2(100, 50));

	image->setFit(UIImageFit::Cover);
	p = image->computePlacement();
	EXPECT_EQ(p.rect.size, Vector2(100, 100));
	EXPECT_FLOAT_EQ(p.uvMin.x, 0.25f);
	EXPECT_FLOAT_EQ(p.uvMax.x, 0.75f);

	image->setFit(UIImageFit::Stretch);
	p = image->computePlacement();
	EXPECT_EQ(p.uvMin, Vector2(0));
	EXPECT_EQ(p.uvMax, Vector2(1));
}

TEST(UIImage, NaturalSizeIsTheTextureSize) {
	UIHarness h;
	auto* image = h.root()->add<UIImage>(fakeTexture(64, 32));
	h.ui.layoutNow();
	EXPECT_EQ(image->getSize(), Vector2(64, 32));
}

TEST(UIDrawList, ImagesStartTheirOwnDrawCommand) {
	auto texture = fakeTexture(8, 8);
	UIDrawList list;
	list.rect({{0, 0}, {5, 5}}, Vector4(1));
	list.image(texture.get(), {{0, 0}, {5, 5}}, Vector2(0), Vector2(1), Vector4(1));
	list.rect({{0, 0}, {5, 5}}, Vector4(1));
	ASSERT_EQ(list.commands().size(), 3u);
	EXPECT_EQ(list.commands()[0].texture, nullptr);
	EXPECT_EQ(list.commands()[1].texture, texture.get());
	EXPECT_EQ(list.commands()[1].firstIndex, 6u);
	EXPECT_EQ(list.commands()[2].indexCount, 6u);
	EXPECT_FLOAT_EQ(list.vertices()[4].style.w, 1.0f);	// image mode
}

TEST(UIDrawList, GradientsCarryPerVertexColours) {
	UIDrawList list;
	list.gradientRect({{0, 0}, {10, 10}}, {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1},
					  {1, 1, 1, 1});
	ASSERT_EQ(list.vertices().size(), 4u);
	EXPECT_EQ(list.vertices()[0].color, Vector4(1, 0, 0, 1));  // top-left
	EXPECT_EQ(list.vertices()[1].color, Vector4(0, 0, 1, 1));  // bottom-left
	EXPECT_EQ(list.vertices()[2].color, Vector4(0, 1, 0, 1));  // top-right
	list.triangle({0, 0}, {1, 0}, {0, 1}, {1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1});
	EXPECT_EQ(list.vertices()[6].color, Vector4(0, 0, 1, 1));
}

// --------------------------------------------------------------- splitter

TEST(UISplitter, DividesDragsClampsAndResets) {
	UIHarness h;
	auto* split = h.root()->add<UISplitter>(UIAxis::Horizontal, 0.5f);
	split->setSize({406, 100});	 // 400px for panes + 6px divider
	split->setMinPaneSize(40);
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(split->getFirst()->getSize().x, 200);
	EXPECT_FLOAT_EQ(split->getSecond()->getLayoutPosition().x, 206);

	const Vector2 divider = h.atLocal(split, {203, 50});
	h.ui.mouseButton(UIMouseButton::Left, true, divider);
	h.ui.mouseMove(divider - Vector2(100, 0));
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(split->getFirst()->getSize().x, 100);
	h.ui.mouseMove(divider - Vector2(1000, 0));	 // clamped by the minimum
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(split->getFirst()->getSize().x, 40);
	h.ui.mouseButton(UIMouseButton::Left, false, divider - Vector2(1000, 0));

	const Vector2 moved = h.atLocal(split, {43, 50});
	h.click(moved);
	h.click(moved);	 // double click restores the initial ratio
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(split->getRatio(), 0.5f);
}

TEST(UISplitter, ResizeCursorOnlyOverTheDivider) {
	UIHarness h;
	auto* split = h.root()->add<UISplitter>(UIAxis::Vertical, 0.5f);
	split->setSize({100, 206});
	h.ui.mouseMove(h.atLocal(split, {50, 103}));
	EXPECT_EQ(h.platform.cursor, UICursor::ResizeVertical);
	h.ui.mouseMove(h.atLocal(split, {50, 20}));
	EXPECT_EQ(h.platform.cursor, UICursor::Arrow);
}
