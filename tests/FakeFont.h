#ifndef BIRD_TESTS_FAKEFONT_H
#define BIRD_TESTS_FAKEFONT_H

#include <memory>
#include <vector>

#include "renders/generic/Font.h"

namespace bird::test {

/**
 * Synthetic font without a GPU atlas: baked at 10px, ascent 8, descent -2,
 * a 100x100 atlas, every glyph 4x6 pixels advancing 5, and space with no
 * bitmap advancing 3.
 */
inline std::shared_ptr<Font> makeFakeFont() {
	format::FontHeader header{};
	header.version = format::FONT_FORMAT_VERSION;
	header.bakedPixelHeight = 10.0f;
	header.ascent = 8.0f;
	header.descent = -2.0f;
	header.atlasWidth = 100;
	header.atlasHeight = 100;
	header.firstCodepoint = 32;
	header.glyphCount = 95;
	header.whiteTexelX = 1;
	header.whiteTexelY = 98;

	std::vector<format::FontGlyph> glyphs(header.glyphCount,
										  {10, 20, 14, 26, 0.5f, -6.0f, 5.0f});
	glyphs[0] = {0, 0, 0, 0, 0.0f, 0.0f, 3.0f};	 // space
	return std::make_shared<Font>(header, std::move(glyphs), nullptr);
}

}  // namespace bird::test

#endif	// !BIRD_TESTS_FAKEFONT_H
