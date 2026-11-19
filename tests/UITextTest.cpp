#include <gtest/gtest.h>

#include <stdexcept>

#include "FakeFont.h"

using bird::Vector2;

TEST(Font, MeasureSumsAdvancesAndScales) {
	const auto font = bird::test::makeFakeFont();
	const Vector2 box = font->measureLine("A B", 20.0f);  // 2x baked size
	EXPECT_FLOAT_EQ(box.x, (5 + 3 + 5) * 2.0f);
	EXPECT_FLOAT_EQ(box.y, (8 + 2) * 2.0f);
}

TEST(Font, LayoutSkipsWhitespaceAndPlacesOnBaseline) {
	const auto font = bird::test::makeFakeFont();
	const auto quads = font->layoutLine("A B", 10.0f);
	ASSERT_EQ(quads.size(), 2u);
	EXPECT_FLOAT_EQ(quads[0].position.x, 0.5f);
	EXPECT_FLOAT_EQ(quads[0].position.y, 8.0f - 6.0f);
	EXPECT_FLOAT_EQ(quads[1].position.x, 5.0f + 3.0f + 0.5f);
	EXPECT_FLOAT_EQ(quads[0].size.x, 4.0f);
	EXPECT_FLOAT_EQ(quads[0].size.y, 6.0f);
	EXPECT_FLOAT_EQ(quads[0].uvMin.x, 0.10f);
	EXPECT_FLOAT_EQ(quads[0].uvMax.y, 0.26f);
}

TEST(Font, UnknownCharactersFallBackToQuestionMark) {
	const auto font = bird::test::makeFakeFont();
	EXPECT_EQ(font->layoutLine("\xC3\xA9", 10.0f).size(), 2u);
}

TEST(Font, WhiteUVIsTexelCentre) {
	const Vector2 uv = bird::test::makeFakeFont()->getWhiteUV();
	EXPECT_FLOAT_EQ(uv.x, 0.015f);
	EXPECT_FLOAT_EQ(uv.y, 0.985f);
}

TEST(Font, RejectsMismatchedGlyphCount) {
	bird::format::FontHeader header{};
	header.version = bird::format::FONT_FORMAT_VERSION;
	header.bakedPixelHeight = 10;
	header.atlasWidth = header.atlasHeight = 8;
	header.firstCodepoint = 32;
	header.glyphCount = 95;
	EXPECT_THROW(bird::Font(header, {}, nullptr), std::invalid_argument);
}

TEST(Font, CaretOffsetsArePrefixAdvances) {
	const auto font = bird::test::makeFakeFont();
	const std::vector<float> offsets = font->caretOffsets("A B", 10.0f);
	ASSERT_EQ(offsets.size(), 4u);
	EXPECT_FLOAT_EQ(offsets[0], 0.0f);
	EXPECT_FLOAT_EQ(offsets[1], 5.0f);
	EXPECT_FLOAT_EQ(offsets[2], 8.0f);	// space advances 3
	EXPECT_FLOAT_EQ(offsets[3], 13.0f);
}
