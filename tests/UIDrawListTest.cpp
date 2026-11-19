#include <gtest/gtest.h>

#include <stdexcept>

#include "UIHarness.h"
#include "scene/2D/UIButton.h"
#include "scene/2D/UIDrawList.h"
#include "scene/2D/UIGeometry.h"
#include "scene/2D/UIScrollView.h"
#include "scene/2D/UIStack.h"

using namespace bird;

TEST(UIDrawList, RectEmitsOneTransformedQuad) {
	UIDrawList list;
	list.setTransform(ui::makeBasis({10, 20}, 0.0f));
	list.rect({{0, 0}, {5, 5}}, Vector4(1.0f));
	ASSERT_EQ(list.vertices().size(), 4u);
	EXPECT_EQ(list.indices().size(), 6u);
	EXPECT_FLOAT_EQ(list.vertices()[0].position.x, 10);
	EXPECT_FLOAT_EQ(list.vertices()[3].position.y, 25);
}

TEST(UIDrawList, NestedClipsIntersectInScreenSpace) {
	UIDrawList list;
	list.setTransform(ui::makeBasis({10, 20}, 0.0f));
	list.pushClip({{0, 0}, {50, 50}});
	list.pushClip({{25, 25}, {100, 100}});
	const UIRect clip = list.currentClip();
	EXPECT_FLOAT_EQ(clip.position.x, 35);
	EXPECT_FLOAT_EQ(clip.position.y, 45);
	EXPECT_FLOAT_EQ(clip.size.x, 25);
	list.rect({{0, 0}, {1, 1}}, Vector4(1.0f));
	EXPECT_FLOAT_EQ(list.vertices()[0].clip.z, 60);	// maxX
	list.popClip();
	list.popClip();
	EXPECT_THROW(list.popClip(), std::logic_error);
}

TEST(UIDrawList, FullyClippedOrTransparentEmitsNothing) {
	UIDrawList list;
	list.rect({{0, 0}, {5, 5}}, Vector4(1, 1, 1, 0));
	list.pushClip({{0, 0}, {0, 0}});
	list.rect({{0, 0}, {5, 5}}, Vector4(1.0f));
	EXPECT_TRUE(list.vertices().empty());
}

TEST(UIDrawList, ScrollViewContentCarriesViewportClip) {
	bird::test::UIHarness h;
	auto* scroll = h.root()->add<UIScrollView>();
	scroll->setSize({200, 100});
	auto* list = scroll->setContent<UIStack>(UIAxis::Vertical);
	for (int i = 0; i < 10; i++) list->add<UIButton>("b");
	h.frame();

	int clipped = 0;
	for (const UIVertex& v : h.ui.getDrawList().vertices()) {
		if (v.clip == Vector4(0, 0, 200, 100)) clipped++;
	}
	EXPECT_GT(clipped, 0);
}

TEST(UIDrawList, RoundedRectCarriesShapeAndClampsRadius) {
	UIDrawList list;
	list.roundedRect({{0, 0}, {20, 10}}, Vector4(1.0f), 50.0f, 1.0f,
					 Vector4(0, 0, 1, 1));
	ASSERT_EQ(list.vertices().size(), 4u);
	const UIVertex& v = list.vertices()[0];
	EXPECT_EQ(v.shape, Vector4(-10, -5, 10, 5));  // local xy, half size
	EXPECT_FLOAT_EQ(v.style.x, 5.0f);			  // radius <= half height
	EXPECT_FLOAT_EQ(v.style.y, 1.0f);
	EXPECT_EQ(v.borderColor, Vector4(0, 0, 1, 1));
}

TEST(UIDrawList, BorderOnlyRoundedRectStillEmits) {
	UIDrawList list;
	list.roundedRect({{0, 0}, {20, 10}}, Vector4(0.0f), 4.0f, 1.0f,
					 Vector4(1.0f));
	EXPECT_EQ(list.vertices().size(), 4u);
	list.roundedRect({{0, 0}, {20, 10}}, Vector4(0.0f), 4.0f);	// invisible
	EXPECT_EQ(list.vertices().size(), 4u);
}

TEST(UIDrawList, ShadowQuadIsEnlargedByBlur) {
	UIDrawList list;
	list.shadow({{10, 10}, {20, 20}}, 4.0f, 5.0f, Vector4(0, 0, 0, 0.5f));
	ASSERT_EQ(list.vertices().size(), 4u);
	const UIVertex& v = list.vertices()[0];
	EXPECT_EQ(v.position, Vector2(5, 5));
	EXPECT_EQ(v.shape, Vector4(-15, -15, 10, 10));
	EXPECT_FLOAT_EQ(v.style.z, 5.0f);  // softness
}

TEST(UIDrawList, TriangleAndLinePrimitives) {
	UIDrawList list;
	list.triangle({0, 0}, {10, 0}, {5, 8}, Vector4(1.0f));
	EXPECT_EQ(list.vertices().size(), 3u);
	EXPECT_EQ(list.indices().size(), 3u);
	EXPECT_EQ(list.vertices()[0].shape.z, 0.0f);  // no shape

	list.line({0, 0}, {10, 0}, 2.0f, Vector4(1.0f));
	ASSERT_EQ(list.vertices().size(), 7u);
	const UIVertex& v = list.vertices()[3];
	EXPECT_FLOAT_EQ(v.shape.z, 6.0f);  // half length 5 + half thickness 1
	EXPECT_FLOAT_EQ(v.shape.w, 1.0f);
	EXPECT_FLOAT_EQ(v.style.x, 1.0f);  // capsule radius
}
