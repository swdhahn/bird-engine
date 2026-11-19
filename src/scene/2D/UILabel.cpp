#include "UILabel.h"

#include <stdexcept>

#include "UITextUtil.h"

namespace bird {

UILabel::UILabel(std::string text) : m_text(std::move(text)) {}

UILabel& UILabel::setText(std::string text) {
	if (text != m_text) {
		m_text = std::move(text);
		markLayoutDirty();
	}
	return *this;
}

UILabel& UILabel::setFontSize(float pixelHeight) {
	if (pixelHeight <= 0.0f) {
		throw std::invalid_argument("UILabel: font size must be positive");
	}
	m_fontSize = pixelHeight;
	markLayoutDirty();
	return *this;
}

float UILabel::getFontSize() const {
	return m_fontSize.value_or(theme().fontSize);
}

UILabel& UILabel::setColor(const Vector4& color) {
	m_color = color;
	markVisualDirty();
	return *this;
}

Vector2 UILabel::measureContent(Vector2 /*available*/) {
	return ui::measureText(font(), m_text, getFontSize());
}

void UILabel::draw(UIDrawList& list) {
	if (!font() || m_text.empty()) return;
	const Vector4 color = isEffectivelyEnabled()
							  ? m_color.value_or(theme().textColor)
							  : theme().disabledText;
	ui::drawTextCenteredY(list, font(), m_text, getFontSize(),
						  getPadding().left,
						  {getPadding().start(), contentSize()}, color);
}

}  // namespace bird
