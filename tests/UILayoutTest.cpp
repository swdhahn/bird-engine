#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UILabel.h"
#include "scene/2D/UIPanel.h"
#include "scene/2D/UIStack.h"

using namespace bird;
using bird::test::UIHarness;

namespace {

void expectVec(const Vector2& actual, const Vector2& expected) {
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
}

UIObject* fixedBox(UIObject* parent, Vector2 size) {
	auto* box = parent->add<UIObject>();
	box->setSize(size);
	return box;
}

}  // namespace

TEST(UILayout, StackAppliesPaddingGapAndMargins) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setPadding(UIInsets::all(10.0f)).setPivot(Vector2(0.0f));
	stack->setGap(5.0f);
	auto* a = fixedBox(stack, {100, 20});
	auto* b = fixedBox(stack, {60, 20});
	b->setMargin({4, 3, 0, 0});
	h.ui.layoutNow();

	expectVec(stack->getSize(), {120, 68});	 // 10+100+10, 10+20+5+3+20+10
	expectVec(a->getLayoutPosition(), {10, 10});
	expectVec(b->getLayoutPosition(), {14, 38});
}

TEST(UILayout, FillChildrenShareSpaceByWeight) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Horizontal);
	stack->setGap(0.0f);
	stack->setSize({300, 40});
	UIObject* boxes[3];
	const float weights[3] = {1, 2, 1};
	for (int i = 0; i < 3; i++) {
		boxes[i] = stack->add<UIObject>();
		boxes[i]->setWidth(UISizeRule::fill(weights[i]));
		boxes[i]->setHeight(UISizeRule::fixed(10));
	}
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(boxes[0]->getSize().x, 75);
	EXPECT_FLOAT_EQ(boxes[1]->getSize().x, 150);
	EXPECT_FLOAT_EQ(boxes[2]->getLayoutPosition().x, 225);
}

TEST(UILayout, JustifyDistributesFreeSpace) {
	const std::pair<UIJustify, std::pair<float, float>> cases[] = {
		{UIJustify::Start, {0, 60}},
		{UIJustify::Center, {95, 155}},
		{UIJustify::End, {190, 250}},
		{UIJustify::SpaceBetween, {0, 250}}};
	for (const auto& [justify, expected] : cases) {
		UIHarness h;
		auto* stack = h.root()->add<UIStack>(UIAxis::Horizontal);
		stack->setGap(10.0f);
		stack->setJustify(justify);
		stack->setSize({300, 40});
		auto* a = fixedBox(stack, {50, 10});
		auto* b = fixedBox(stack, {50, 10});
		h.ui.layoutNow();
		EXPECT_FLOAT_EQ(a->getLayoutPosition().x, expected.first);
		EXPECT_FLOAT_EQ(b->getLayoutPosition().x, expected.second);
	}
}

TEST(UILayout, CrossAxisAlignCentersAndStretches) {
	UIHarness h;
	auto* centered = h.root()->add<UIStack>(UIAxis::Vertical);
	centered->setAlign(UIAlign::Center);
	centered->setSize({200, 100});
	auto* small = fixedBox(centered, {50, 10});

	auto* stretched = h.root()->add<UIStack>(UIAxis::Vertical);
	stretched->setAlign(UIAlign::Stretch);
	stretched->setSize({200, 100});
	auto* fit = stretched->add<UILabel>("ab");
	auto* fixed = fixedBox(stretched, {50, 10});
	h.ui.layoutNow();

	EXPECT_FLOAT_EQ(small->getLayoutPosition().x, 75);
	EXPECT_FLOAT_EQ(fit->getSize().x, 200);	 // Fit stretches
	EXPECT_FLOAT_EQ(fixed->getSize().x, 50);  // Fixed keeps its size
}

TEST(UILayout, FillRespectsMaximum) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Horizontal);
	stack->setGap(0.0f);
	stack->setSize({300, 20});
	UISizeRule capped = UISizeRule::fill();
	capped.max = 100;
	auto* a = stack->add<UIObject>();
	a->setWidth(capped);
	auto* b = stack->add<UIObject>();
	b->setWidth(UISizeRule::fill());
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(a->getSize().x, 100);
	EXPECT_FLOAT_EQ(b->getSize().x, 150);
	EXPECT_FLOAT_EQ(b->getLayoutPosition().x, 100);
}

TEST(UILayout, PanelFitsContentPlusPadding) {
	UIHarness h;
	auto* panel = h.root()->add<UIPanel>();	// theme padding 8
	panel->add<UILabel>("abc");				// 3 x 7px wide, 14px tall
	h.ui.layoutNow();
	expectVec(panel->getSize(), {21 + 16, 14 + 16});
}

TEST(UILayout, StretchAnchorsHonourMargins) {
	UIHarness h;
	auto* child = h.root()->add<UIObject>();
	child->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	child->setMargin(UIInsets::all(10));
	h.ui.layoutNow();
	expectVec(child->getLayoutPosition(), {10, 10});
	expectVec(child->getSize(), {780, 580});
}

TEST(UILayout, CentredAnchorFollowsViewportResize) {
	UIHarness h;
	auto* box = fixedBox(h.root(), {100, 50});
	box->setAnchor(UIAlignment::Center).setPivot(UIAlignment::Center);
	h.ui.layoutNow();
	expectVec(box->getLayoutPosition(), {350, 275});

	h.ui.setViewportSize(400, 300);
	h.ui.layoutNow();
	expectVec(box->getLayoutPosition(), {150, 125});
}

TEST(UILayout, BottomRightAnchorWithNegativeOffset) {
	UIHarness h;
	auto* box = fixedBox(h.root(), {100, 30});
	box->setAnchor(UIAlignment::BottomRight).setPivot(UIAlignment::BottomRight);
	box->setOffset({-16, -16});
	h.ui.layoutNow();
	expectVec(box->getLayoutPosition(), {684, 554});
}

TEST(UILayout, HiddenChildrenTakeNoSpace) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setGap(5.0f);
	fixedBox(stack, {10, 10});
	fixedBox(stack, {10, 10})->setVisible(false);
	fixedBox(stack, {10, 10});
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(stack->getSize().y, 25);  // 10 + 5 + 10
}

TEST(UIStackAnchors, CrossAxisAnchorPlacesChild) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setSize({200, 100});
	auto* child = fixedBox(stack, {50, 10});
	child->setAnchor(UIAlignment::TopRight).setPivot(UIAlignment::TopRight);
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(child->getLayoutPosition().x, 150);
}

TEST(UIStackAnchors, UnequalCrossAnchorsStretch) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setSize({200, 100});
	auto* child = stack->add<UIObject>();
	child->setHeight(UISizeRule::fixed(10));
	child->setAnchors({0.25f, 0.0f}, {0.75f, 0.0f});
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(child->getLayoutPosition().x, 50);
	EXPECT_FLOAT_EQ(child->getSize().x, 100);
}

TEST(UIStackAnchors, OffsetNudgesWithoutMovingSiblings) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setGap(0.0f);
	auto* a = fixedBox(stack, {10, 10});
	a->setOffset({5, 7});
	auto* b = fixedBox(stack, {10, 10});
	h.ui.layoutNow();
	expectVec(a->getLayoutPosition(), {5, 7});
	expectVec(b->getLayoutPosition(), {0, 10});
}

TEST(UIStackAnchors, AbsoluteChildTakesNoSpace) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setGap(5.0f);
	stack->setWidth(UISizeRule::fixed(100));
	fixedBox(stack, {10, 10});
	fixedBox(stack, {10, 10});
	auto* badge = fixedBox(stack, {20, 20});
	badge->setLayoutMode(UILayoutMode::Absolute);
	badge->setAnchor(UIAlignment::TopRight).setPivot(UIAlignment::TopRight);
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(stack->getSize().y, 25);  // badge excluded
	expectVec(badge->getLayoutPosition(), {80, 0});
}

TEST(UIStackAnchors, DefaultAnchorsKeepStackAlign) {
	UIHarness h;
	auto* stack = h.root()->add<UIStack>(UIAxis::Vertical);
	stack->setAlign(UIAlign::Center);
	stack->setSize({200, 100});
	auto* child = fixedBox(stack, {50, 10});
	h.ui.layoutNow();
	EXPECT_FLOAT_EQ(child->getLayoutPosition().x, 75);	// align, not TopLeft
}
