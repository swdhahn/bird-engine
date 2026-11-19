#include "UITooltip.h"

#include "UITextUtil.h"

namespace bird {

UITooltip::UITooltip(std::string text) : m_text(std::move(text)) {
	setPadding(UIInsets::symmetric(theme().padding, theme().padding * 0.5f));
}

Vector2 UITooltip::measureContent(Vector2 /*available*/) {
	return ui::measureText(font(), m_text, theme().fontSize);
}

void UITooltip::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.shadow(bounds, t.popupRadius, t.shadowBlur * 0.5f, t.shadowColor);
	list.roundedRect(bounds, t.tooltipColor, t.popupRadius, t.borderWidth,
					 t.borderColor);
	ui::drawTextCenteredY(list, font(), m_text, t.fontSize, getPadding().left,
						  bounds, t.textColor);
}

}  // namespace bird
