#ifndef BIRD_UILABEL_H
#define BIRD_UILABEL_H

#include <memory>
#include <string>

#include "UIObject.h"

namespace bird {

class Font;

/**
 * A single line of text. The label sizes itself: its scale always equals
 * the measured text box, so setScale on a label is overwritten on layout.
 * Labels are not interactive and never intercept mouse events.
 */
class UILabel : public UIObject {
   public:
	/** @param text - initial text; ASCII only. */
	explicit UILabel(std::string text = "");
	~UILabel();

	void setText(std::string text);
	const std::string& getText() const;
	/** @param pixelHeight - must be positive. */
	void setFontSize(float pixelHeight);
	void setColor(const Vector3& color);
	/** Overrides the UIManager's default font; nullptr restores it. */
	void setFont(std::shared_ptr<Font> font);

	/** Rebuilds glyph quads when text, size, colour or font changed. */
	void layout() override;
	/**
	 * Rebuilds glyph quads from an explicit font. Exposed for testing.
	 * @throws std::invalid_argument when font is nullptr.
	 */
	void rebuildGlyphs(const std::shared_ptr<Font>& font);

   private:
	void markDirty();

	std::string m_text;
	std::shared_ptr<Font> m_font;
	Vector3 m_color;
	float m_fontSize;
	bool m_dirty = true;
};

}  // namespace bird

#endif	// !BIRD_UILABEL_H
