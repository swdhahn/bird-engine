#ifndef BIRD_UILABEL_H
#define BIRD_UILABEL_H

#include <optional>
#include <string>

#include "UIObject.h"

namespace bird {

/**
 * A single line of text. Sizes itself to the text (Fit) unless a size rule
 * says otherwise. Not interactive: pointer events pass through to whatever
 * lies beneath.
 */
class UILabel : public UIObject {
   public:
	/** @param text - ASCII text; other characters render as '?'. */
	explicit UILabel(std::string text = "");

	UILabel& setText(std::string text);
	const std::string& getText() const { return m_text; }
	/**
	 * @param pixelHeight - must be positive.
	 * @throws std::invalid_argument otherwise.
	 */
	UILabel& setFontSize(float pixelHeight);
	float getFontSize() const;
	/** Overrides the theme's text colour. */
	UILabel& setColor(const Vector4& color);

	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	std::string m_text;
	std::optional<float> m_fontSize;
	std::optional<Vector4> m_color;
};

}  // namespace bird

#endif	// !BIRD_UILABEL_H
