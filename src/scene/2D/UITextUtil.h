#ifndef BIRD_UITEXTUTIL_H
#define BIRD_UITEXTUTIL_H

#include <string>

#include "../../renders/generic/Font.h"
#include "UIDrawList.h"

namespace bird::ui {

/** @return the text box size, or zero when no font is available. */
inline Vector2 measureText(const Font* font, const std::string& text,
						   float pixelHeight) {
	return font ? font->measureLine(text, pixelHeight) : Vector2(0.0f);
}

/** Draws text vertically centred in a rect, at the given horizontal x. */
inline void drawTextCenteredY(UIDrawList& list, const Font* font,
							  const std::string& text, float pixelHeight,
							  float x, const UIRect& rect,
							  const Vector4& color) {
	if (!font || text.empty()) return;
	const float lineHeight = font->getLineHeight(pixelHeight);
	list.text(*font, text,
			  Vector2(x, rect.position.y + (rect.size.y - lineHeight) * 0.5f),
			  pixelHeight, color);
}

/** Draws text centred on both axes within a rect. */
inline void drawTextCentered(UIDrawList& list, const Font* font,
							 const std::string& text, float pixelHeight,
							 const UIRect& rect, const Vector4& color) {
	if (!font || text.empty()) return;
	const Vector2 size = font->measureLine(text, pixelHeight);
	drawTextCenteredY(list, font, text, pixelHeight,
					  rect.position.x + (rect.size.x - size.x) * 0.5f, rect,
					  color);
}

}  // namespace bird::ui

#endif	// !BIRD_UITEXTUTIL_H
