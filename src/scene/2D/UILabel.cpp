#include "UILabel.h"

#include <stdexcept>

#include "../../renders/generic/Font.h"
#include "../Application.h"
#include "UIManager.h"

namespace bird {

UILabel::UILabel(std::string text)
	: UIObject(0),
	  m_text(std::move(text)),
	  m_color(m_theme->textColor),
	  m_fontSize(m_theme->fontSize) {
	m_interactive = false;
}
UILabel::~UILabel() {}

void UILabel::setText(std::string text) {
	if (text == m_text) return;
	m_text = std::move(text);
	markDirty();
}

const std::string& UILabel::getText() const {
	return m_text;
}

void UILabel::setFontSize(float pixelHeight) {
	if (pixelHeight <= 0.0f) {
		throw std::invalid_argument("UILabel: font size must be positive");
	}
	m_fontSize = pixelHeight;
	markDirty();
}

void UILabel::setColor(const Vector3& color) {
	m_color = color;
	markDirty();
}

void UILabel::setFont(std::shared_ptr<Font> font) {
	m_font = std::move(font);
	markDirty();
}

void UILabel::markDirty() {
	m_dirty = true;
	if (APPLICATION) APPLICATION->getUIManager()->setUpdateUIFlag();
}

void UILabel::layout() {
	if (!m_dirty) return;
	rebuildGlyphs(m_font ? m_font : APPLICATION->getUIManager()->getFont());
}

void UILabel::rebuildGlyphs(const std::shared_ptr<Font>& font) {
	if (!font) {
		throw std::invalid_argument("UILabel: no font available");
	}
	const Vector2 box = font->measureLine(m_text, m_fontSize);
	setScale(box);
	m_drawables.clear();
	m_dirty = false;
	if (box.x <= 0.0f || box.y <= 0.0f) return;

	// Glyph quads arrive in pixels; normalize them into the label's unit
	// space, which the world transform scales back by the box size.
	for (const GlyphQuad& glyph : font->layoutLine(m_text, m_fontSize)) {
		UIDrawable drawable;
		drawable.localOffset = glyph.position / box;
		drawable.size = glyph.size / box;
		drawable.color = m_color;
		drawable.textured = true;
		drawable.uvMin = glyph.uvMin;
		drawable.uvMax = glyph.uvMax;
		m_drawables.push_back(drawable);
	}
}

}  // namespace bird
