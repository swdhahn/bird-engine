#include <gtest/gtest.h>

#include <stdexcept>

#include "UIHarness.h"
#include "scene/2D/UIAnimator.h"
#include "scene/2D/UIPanel.h"
#include "scene/2D/UIToggle.h"

using namespace bird;
using namespace bird::test;

namespace {

UIObject* addBox(UIHarness& h, Vector2 position = Vector2(100.0f)) {
	auto* box = h.root()->add<UIPanel>();
	box->setSize(Vector2(100.0f, 50.0f));
	box->setLayoutMode(UILayoutMode::Absolute);
	box->setOffset(position);
	h.frame();
	return box;
}

float maxVertexAlpha(const UIDrawList& list) {
	float alpha = 0.0f;
	for (const UIVertex& v : list.vertices()) alpha = std::max(alpha, v.color.a);
	return alpha;
}

}  // namespace

TEST(UIAnimator, FadesOpacityAndScalesDrawAlpha) {
	UIHarness h;
	UIObject* box = addBox(h);
	const float opaqueAlpha = maxVertexAlpha(h.ui.getDrawList());
	ASSERT_GT(opaqueAlpha, 0.0f);

	box->animate().opacity(1.0f, 0.0f).duration(1.0f).ease(Ease::Linear).start();
	h.frame(0.5f);
	EXPECT_NEAR(box->getOpacity(), 0.5f, 1e-4f);
	EXPECT_NEAR(maxVertexAlpha(h.ui.getDrawList()), opaqueAlpha * 0.5f, 1e-4f);
	h.frame(0.6f);
	EXPECT_FLOAT_EQ(box->getOpacity(), 0.0f);
	EXPECT_TRUE(h.ui.getDrawList().vertices().empty());	 // culled entirely
}

TEST(UIAnimator, OpacityMultipliesThroughTheSubtree) {
	UIHarness h;
	UIObject* box = addBox(h);
	auto* child = box->add<UIPanel>();
	child->setSize(Vector2(10.0f));
	box->setOpacity(0.5f);
	child->setOpacity(0.5f);
	h.frame();
	const auto& vertices = h.ui.getDrawList().vertices();
	ASSERT_FALSE(vertices.empty());
	// The deepest panel is drawn last: alpha scaled by 0.25.
	box->setOpacity(1.0f);
	child->setOpacity(1.0f);
	h.frame();
	const float full = h.ui.getDrawList().vertices().back().color.a;
	box->setOpacity(0.5f);
	child->setOpacity(0.5f);
	h.frame();
	EXPECT_NEAR(h.ui.getDrawList().vertices().back().color.a, full * 0.25f,
				1e-5f);
}

TEST(UIAnimator, VisualTransformsSkipLayout) {
	UIHarness h;
	UIObject* box = addBox(h);
	box->setVisualOffset(Vector2(30.0f, 0.0f));
	EXPECT_FALSE(h.ui.isLayoutDirty());
	h.frame();
	EXPECT_EQ(box->getLayoutPosition(), Vector2(100.0f));  // layout untouched
	EXPECT_NEAR(h.at(box).x, 100.0f + 50.0f + 30.0f, 1e-4f);
}

TEST(UIAnimator, ScaleIsAroundTheCentreAndHitTestingFollows) {
	UIHarness h;
	UIObject* box = addBox(h);
	box->setVisualScale(Vector2(0.5f));
	h.frame();
	const UIRect rect = box->getScreenRect();
	EXPECT_NEAR(rect.position.x, 125.0f, 1e-4f);
	EXPECT_NEAR(rect.size.x, 50.0f, 1e-4f);
	EXPECT_NEAR(rect.size.y, 25.0f, 1e-4f);
	EXPECT_FALSE(box->containsPoint(Vector2(105.0f, 105.0f)));	// was inside
	EXPECT_TRUE(box->containsPoint(Vector2(150.0f, 125.0f)));
}

TEST(UIAnimator, ImplicitStartValuesAndOvershootEasing) {
	UIHarness h;
	UIObject* box = addBox(h);
	box->setVisualScale(Vector2(0.5f));
	box->animate().scale(1.0f).duration(1.0f).ease(Ease::OutBack).start();
	float peak = 0.0f;
	for (int i = 0; i < 20; ++i) {
		h.frame(0.05f);
		peak = std::max(peak, box->getVisualScale().x);
	}
	EXPECT_GT(peak, 1.0f);	// OutBack overshoot survives to the property
	EXPECT_FLOAT_EQ(box->getVisualScale().x, 1.0f);
}

TEST(UIAnimator, NewerTransitionTakesOverOnlyItsProperties) {
	UIHarness h;
	UIObject* box = addBox(h);
	box->animate()
		.opacity(1.0f, 0.0f)
		.offset(Vector2(0.0f), Vector2(100.0f, 0.0f))
		.duration(1.0f)
		.ease(Ease::Linear)
		.start();
	h.frame(0.5f);
	box->animate().opacity(1.0f).duration(1.0f).ease(Ease::Linear).start();
	h.frame(0.25f);
	// Offset still follows the first transition; opacity the second.
	EXPECT_NEAR(box->getVisualOffset().x, 75.0f, 1e-3f);
	EXPECT_NEAR(box->getOpacity(), 0.5f + 0.5f * 0.25f, 1e-3f);
	h.frame(1.0f);
	EXPECT_FLOAT_EQ(box->getOpacity(), 1.0f);
	EXPECT_FLOAT_EQ(box->getVisualOffset().x, 100.0f);
}

TEST(UIAnimator, DelayHoldsTheOldStateThenFinishes) {
	UIHarness h;
	UIObject* box = addBox(h);
	int finished = 0;
	box->animate()
		.opacity(0.0f)
		.delay(0.5f)
		.duration(0.5f)
		.onFinished([&] { ++finished; })
		.start();
	h.frame(0.4f);
	EXPECT_FLOAT_EQ(box->getOpacity(), 1.0f);
	h.frame(1.0f);
	EXPECT_FLOAT_EQ(box->getOpacity(), 0.0f);
	EXPECT_EQ(finished, 1);
	EXPECT_EQ(h.ui.getAnimationSystem().getActiveCount(), 0u);
}

TEST(UIAnimator, RemovingANodeCancelsItsSubtreeTransitions) {
	UIHarness h;
	UIObject* box = addBox(h);
	auto* child = box->add<UIPanel>();
	child->animate().opacity(0.0f).duration(1.0f).start();
	box->animate().rotation(1.0f).duration(1.0f).start();
	EXPECT_EQ(h.ui.getAnimationSystem().getActiveCount(), 2u);
	box->removeFromParent();
	h.frame(0.1f);	// would write through dangling pointers if not cancelled
	EXPECT_EQ(h.ui.getAnimationSystem().getActiveCount(), 0u);
}

TEST(UIAnimator, DetachedNodeSnapsToFinalValues) {
	UIPanel panel;
	int finished = 0;
	const AnimationHandle handle = panel.animate()
									   .opacity(0.25f)
									   .offset(Vector2(4.0f, 2.0f))
									   .onFinished([&] { ++finished; })
									   .start();
	EXPECT_TRUE(handle.isNull());
	EXPECT_FLOAT_EQ(panel.getOpacity(), 0.25f);
	EXPECT_EQ(panel.getVisualOffset(), Vector2(4.0f, 2.0f));
	EXPECT_EQ(finished, 1);
}

TEST(UIAnimator, MisuseFailsFast) {
	UIHarness h;
	UIObject* box = addBox(h);
	EXPECT_THROW(box->animate().start(), std::logic_error);
	UIAnimator animator = box->animate().opacity(0.0f);
	animator.start();
	EXPECT_THROW(animator.start(), std::logic_error);
	EXPECT_THROW(box->animate().duration(-1.0f), std::invalid_argument);
	EXPECT_THROW(box->animate().bezier(2.0f, 0.0f, 0.5f, 1.0f),
				 std::invalid_argument);
	EXPECT_THROW(box->animate().value(0.0f, 1.0f, nullptr),
				 std::invalid_argument);
}

TEST(UIAnimator, LoopingPingPongKeepsRunning) {
	UIHarness h;
	UIObject* box = addBox(h);
	box->animate()
		.rotation(0.0f, 1.0f)
		.duration(1.0f)
		.ease(Ease::Linear)
		.loop(LoopMode::PingPong)
		.start();
	h.frame(1.5f);
	EXPECT_NEAR(box->getVisualRotation(), 0.5f, 1e-4f);
	h.frame(10.0f);
	EXPECT_EQ(h.ui.getAnimationSystem().getActiveCount(), 1u);
}

TEST(UIToggle, ReversingMidSlideKeepsConstantSpeed) {
	UIHarness h;
	auto* toggle = h.root()->add<UIToggle>("Sound");
	h.frame();
	toggle->setOn(true);
	h.frame(0.06f);	 // half way (animationSeconds = 0.12)
	EXPECT_NEAR(toggle->getKnobPosition(), 0.5f, 1e-4f);
	toggle->setOn(false);
	h.frame(0.03f);
	EXPECT_NEAR(toggle->getKnobPosition(), 0.25f, 1e-4f);
	h.frame(0.03f);
	EXPECT_FLOAT_EQ(toggle->getKnobPosition(), 0.0f);
	EXPECT_EQ(h.ui.getAnimationSystem().getActiveCount(), 0u);
}
