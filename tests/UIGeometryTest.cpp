#include <gtest/gtest.h>

#include "scene/2D/UIGeometry.h"

using bird::Matrix3;
using bird::Vector2;

namespace {

/** Builds a translate-and-scale matrix (no rotation), column-major. */
Matrix3 makeTransform(Vector2 position, Vector2 scale) {
	return Matrix3(scale.x, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, position.x,
				   position.y, 1.0f);
}

void expectVec2Eq(const Vector2& actual, const Vector2& expected) {
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
}

}  // namespace

TEST(UIGeometry, CornersApplyTranslationAndScale) {
	const auto corners = bird::ui::computeDrawableCorners(
		makeTransform({600, 400}, {200, 100}), {0, 0}, {1, 1});
	expectVec2Eq(corners[0], {600, 400});  // top-left
	expectVec2Eq(corners[1], {600, 500});  // bottom-left
	expectVec2Eq(corners[2], {800, 400});  // top-right
	expectVec2Eq(corners[3], {800, 500});  // bottom-right
}

TEST(UIGeometry, DrawableOffsetIsInObjectLocalSpace) {
	const auto corners = bird::ui::computeDrawableCorners(
		makeTransform({600, 400}, {200, 100}), {0.05f, 0.05f}, {0.9f, 0.9f});
	expectVec2Eq(corners[0], {610, 405});
	expectVec2Eq(corners[3], {790, 495});
}

TEST(UIGeometry, NestedTransformsCompose) {
	const Matrix3 parent = makeTransform({100, 50}, {1, 1});
	const Matrix3 child = makeTransform({10, 20}, {30, 40});
	const auto corners =
		bird::ui::computeDrawableCorners(parent * child, {0, 0}, {1, 1});
	expectVec2Eq(corners[0], {110, 70});
	expectVec2Eq(corners[3], {140, 110});
}

TEST(UIGeometry, ContainsInteriorAndEdges) {
	const Matrix3 world = makeTransform({600, 400}, {200, 100});
	EXPECT_TRUE(bird::ui::containsScreenPoint(world, {700, 450}));
	EXPECT_TRUE(bird::ui::containsScreenPoint(world, {600, 400}));
	EXPECT_TRUE(bird::ui::containsScreenPoint(world, {800, 500}));
	EXPECT_FALSE(bird::ui::containsScreenPoint(world, {599, 450}));
	EXPECT_FALSE(bird::ui::containsScreenPoint(world, {700, 501}));
}

TEST(UIGeometry, ContainsRespectsRotation) {
	// 90 degrees: local +x maps to screen +y, local +y maps to screen -x.
	const Matrix3 world(0.0f, 100.0f, 0.0f, -100.0f, 0.0f, 0.0f, 500.0f,
						500.0f, 1.0f);
	EXPECT_TRUE(bird::ui::containsScreenPoint(world, {450, 550}));
	EXPECT_FALSE(bird::ui::containsScreenPoint(world, {550, 550}));
}

TEST(UIGeometry, DegenerateScaleNeverContains) {
	EXPECT_FALSE(bird::ui::containsScreenPoint(makeTransform({0, 0}, {0, 10}),
											   {0, 5}));
}
