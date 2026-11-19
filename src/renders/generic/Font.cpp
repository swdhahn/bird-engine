#include "Font.h"

#include <stdexcept>

#include "Texture.h"

namespace bird {

namespace {
constexpr char kFallbackCharacter = '?';
}

Font::Font(const format::FontHeader& header,
		   std::vector<format::FontGlyph> glyphs,
		   std::shared_ptr<Texture> atlas)
	: m_header(header), m_glyphs(std::move(glyphs)), m_atlas(std::move(atlas)) {
	if (m_header.version != format::FONT_FORMAT_VERSION) {
		throw std::invalid_argument("Font: unsupported format version");
	}
	if (m_header.glyphCount == 0 || m_glyphs.size() != m_header.glyphCount) {
		throw std::invalid_argument("Font: glyph count mismatch");
	}
	if (m_header.atlasWidth == 0 || m_header.atlasHeight == 0 ||
		m_header.bakedPixelHeight <= 0.0f) {
		throw std::invalid_argument("Font: invalid atlas or pixel height");
	}
	const uint32_t fallback = static_cast<uint32_t>(kFallbackCharacter);
	if (fallback < m_header.firstCodepoint ||
		fallback >= m_header.firstCodepoint + m_header.glyphCount) {
		throw std::invalid_argument("Font: fallback glyph '?' is not baked");
	}
}

Font::~Font() {}

void Font::read() {}
void Font::write() {}

const std::shared_ptr<Texture>& Font::getAtlas() const {
	return m_atlas;
}

Vector2 Font::getWhiteUV() const {
	return Vector2((m_header.whiteTexelX + 0.5f) / m_header.atlasWidth,
				   (m_header.whiteTexelY + 0.5f) / m_header.atlasHeight);
}

float Font::scaleFor(float pixelHeight) const {
	return pixelHeight / m_header.bakedPixelHeight;
}

float Font::getLineHeight(float pixelHeight) const {
	return (m_header.ascent - m_header.descent) * scaleFor(pixelHeight);
}

const format::FontGlyph& Font::glyphFor(char character) const {
	uint32_t codepoint = static_cast<unsigned char>(character);
	if (codepoint < m_header.firstCodepoint ||
		codepoint >= m_header.firstCodepoint + m_header.glyphCount) {
		codepoint = static_cast<uint32_t>(kFallbackCharacter);
	}
	return m_glyphs[codepoint - m_header.firstCodepoint];
}

std::vector<GlyphQuad> Font::layoutLine(const std::string& text,
										float pixelHeight) const {
	const float scale = scaleFor(pixelHeight);
	const float baseline = m_header.ascent * scale;
	const Vector2 atlasSize(m_header.atlasWidth, m_header.atlasHeight);

	std::vector<GlyphQuad> quads;
	quads.reserve(text.size());
	float pen = 0.0f;
	for (char character : text) {
		const format::FontGlyph& glyph = glyphFor(character);
		const Vector2 pixelMin(glyph.atlasX0, glyph.atlasY0);
		const Vector2 pixelMax(glyph.atlasX1, glyph.atlasY1);
		if (pixelMax.x > pixelMin.x && pixelMax.y > pixelMin.y) {
			quads.push_back(
				{Vector2(pen + glyph.offsetX * scale,
						 baseline + glyph.offsetY * scale),
				 (pixelMax - pixelMin) * scale, pixelMin / atlasSize,
				 pixelMax / atlasSize});
		}
		pen += glyph.advance * scale;
	}
	return quads;
}

Vector2 Font::measureLine(const std::string& text, float pixelHeight) const {
	float width = 0.0f;
	for (char character : text) {
		width += glyphFor(character).advance;
	}
	return Vector2(width * scaleFor(pixelHeight), getLineHeight(pixelHeight));
}

}  // namespace bird
