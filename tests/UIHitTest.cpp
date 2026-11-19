#include <gtest/gtest.h>

#include <memory>

#include "FakeFont.h"
#include "scene/2D/UILabel.h"
#include "scene/2D/UIManager.h"
#include "scene/2D/UIObject.h"

using bird::Matrix3;
using bird::UIManager;
using bird::UIObject;
using bird::Vector2;

namespace {

/**
 * Appends a single-quad child at the given rectangle. Children are pushed
 * directly so that the test avoids the global APPLICATION dependency of
 * UIObject::addChild.
 */
UIObject* addQuad(UIObject* parent, Vector2 position, Vector2 size) {
	auto child = std::make_unique<UIObject>(1);
	child->getDrawables()[0] = {{0, 0}, {1, 1}, {1, 1, 1}};
	child->setPosition(position);
	child->setScale(size);
	UIObject* raw = child.get();
	parent->getChildren().push_back(std::move(child));
	return raw;
}

/** Recomputes world matrices the way UIManager::updateUI does. */
void layout(UIObject* object, const Matrix3& parentBasis = Matrix3(1.0f),
			const Vector2& parentSize = Vector2(0.0f)) {
	object->updateWorldMatrix(parentBasis, parentSize);
	for (auto& child : object->getChildren()) {
		layout(child.get(), object->getChildBasis(), object->getScale());
	}
}

}  // namespace

TEST(UIHitTest, MissReturnsNull) {
	UIObject root(0);
	addQuad(&root, {0, 0}, {10, 10});
	layout(&root);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {50, 50}), nullptr);
}

TEST(UIHitTest, RootWithoutDrawablesIsNotHittable) {
	UIObject root(0);
	root.setScale({1000, 1000});
	layout(&root);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {5, 5}), nullptr);
}

TEST(UIHitTest, LaterSiblingWinsOverlap) {
	UIObject root(0);
	addQuad(&root, {0, 0}, {100, 100});
	UIObject* top = addQuad(&root, {50, 50}, {100, 100});
	layout(&root);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {75, 75}), top);
}

TEST(UIHitTest, ChildWinsOverParentAndUsesParentTransform) {
	UIObject root(0);
	UIObject* panel = addQuad(&root, {100, 100}, {200, 200});
	UIObject* inner = addQuad(panel, {10, 10}, {20, 20});
	layout(&root);
	// Inner occupies screen [110, 130] because it is offset by the panel.
	EXPECT_EQ(UIManager::findTopmostAt(&root, {120, 120}), inner);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {15, 15}), nullptr);
}

TEST(UIHitTest, ParentScaleIsNotInherited) {
	UIObject root(0);
	UIObject* panel = addQuad(&root, {100, 100}, {200, 200});
	addQuad(panel, {10, 10}, {20, 20});
	layout(&root);
	// Inner stays 20x20 pixels; (135, 135) therefore hits the panel only.
	EXPECT_EQ(UIManager::findTopmostAt(&root, {135, 135}), panel);
}

TEST(UIHitTest, NonInteractiveObjectsArePassedThrough) {
	UIObject root(0);
	UIObject* button = addQuad(&root, {0, 0}, {100, 100});
	auto caption = std::make_unique<bird::UILabel>("AAAAAAAAAAAAAAAAAAAA");
	caption->rebuildGlyphs(bird::test::makeFakeFont());
	ASSERT_GT(caption->getDrawableCount(), 0u);
	button->getChildren().push_back(std::move(caption));
	layout(&root);
	// The caption covers (50, 5) but is not interactive, so the button wins.
	EXPECT_EQ(UIManager::findTopmostAt(&root, {50, 5}), button);
}

TEST(UIAnchor, CentredObjectFollowsViewportResize) {
	UIObject root(0);
	UIObject* box = addQuad(&root, {0, 0}, {200, 100});
	box->setAnchor(bird::UIAlignment::Center);
	box->setPivot(bird::UIAlignment::Center);

	root.setScale({1280, 720});
	layout(&root);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {640, 360}), box);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {539, 360}), nullptr);

	root.setScale({640, 1000});
	layout(&root);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {320, 500}), box);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {640, 360}), nullptr);
}

TEST(UIAnchor, BottomRightAnchorWithNegativeOffset) {
	UIObject root(0);
	UIObject* box = addQuad(&root, {-10, -10}, {50, 20});
	box->setAnchor(bird::UIAlignment::BottomRight);
	box->setPivot(bird::UIAlignment::BottomRight);
	root.setScale({800, 600});
	layout(&root);
	// Occupies [740, 790] x [570, 590].
	EXPECT_EQ(UIManager::findTopmostAt(&root, {745, 575}), box);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {795, 595}), nullptr);
}

TEST(UIAnchor, ChildAnchorsWithinParentSize) {
	UIObject root(0);
	UIObject* panel = addQuad(&root, {100, 100}, {200, 100});
	UIObject* inner = addQuad(panel, {0, 0}, {20, 20});
	inner->setAnchor(bird::UIAlignment::Center);
	inner->setPivot(bird::UIAlignment::Center);
	layout(&root);
	// Panel centre is (200, 150); inner spans [190, 210] on both axes' centre.
	EXPECT_EQ(UIManager::findTopmostAt(&root, {200, 150}), inner);
	EXPECT_EQ(UIManager::findTopmostAt(&root, {185, 150}), panel);
}
