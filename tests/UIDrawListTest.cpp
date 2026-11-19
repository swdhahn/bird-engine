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
