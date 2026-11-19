#ifndef BIRD_FONT_H
#define BIRD_FONT_H

#include <memory>
#include <string>
#include <vector>

#include "../../build-tools/FormatHeaders.h"
#include "../../util/Math.h"
#include "../../util/Resource.h"

namespace bird {

class Texture;

/** One positioned glyph, in pixels relative to the line box's top-left. */
struct GlyphQuad {
	Vector2 position;
	Vector2 size;
	Vector2 uvMin;
	Vector2 uvMax;
};

/**
 * A baked bitmap font: glyph metrics plus an atlas texture produced by
 * AssetLoader::loadFont. Layout is CPU-only, so a Font without an atlas
 * (nullptr) remains valid for measurement and testing.
 */
class Font : public Resource {
   public:
	/**
	 * @param header - metrics and atlas dimensions from the .bird file.
	 * @param glyphs - exactly header.glyphCount entries.
	 * @param atlas - GPU texture of the atlas; may be nullptr.
	 * @throws std::invalid_argument on a version or size mismatch.
	 */
	Font(const format::FontHeader& header,
		 std::vector<format::FontGlyph> glyphs,
		 std::shared_ptr<Texture> atlas);
	~Font() override;

	void read() override;
	void write() override;

	const std::shared_ptr<Texture>& getAtlas() const;
	/** @return the UV of a fully covered texel, for untextured quads. */
	Vector2 getWhiteUV() const;
	/** @return ascent minus descent at the requested pixel height. */
	float getLineHeight(float pixelHeight) const;

	/**
	 * Lays out one line of ASCII text. Characters outside the baked range
	 * render as '?'.
	 * @param text - the line to lay out; newlines are not interpreted.
	 * @param pixelHeight - target font size in pixels.
	 * @return quads for visible glyphs; whitespace advances without a quad.
	 */
	std::vector<GlyphQuad> layoutLine(const std::string& text,
									  float pixelHeight) const;
	/** @return the line box size (total advance, line height) in pixels. */
	Vector2 measureLine(const std::string& text, float pixelHeight) const;

   private:
	const format::FontGlyph& glyphFor(char character) const;
	float scaleFor(float pixelHeight) const;

	format::FontHeader m_header;
	std::vector<format::FontGlyph> m_glyphs;
	std::shared_ptr<Texture> m_atlas;
};

}  // namespace bird

#endif	// !BIRD_FONT_H
