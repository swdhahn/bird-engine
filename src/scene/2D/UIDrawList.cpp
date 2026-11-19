#include "UIDrawList.h"

#include <stdexcept>

#include "../../renders/generic/Font.h"

namespace bird {

UIDrawList::UIDrawList(Vector2 whiteUV) : m_whiteUV(whiteUV) {
	clear();
}

void UIDrawList::clear() {
	m_vertices.clear();
	m_indices.clear();
	m_clipStack.assign(1, UIRect::unbounded());
	m_transform = Matrix3(1.0f);
}

void UIDrawList::setWhiteUV(Vector2 whiteUV) {
	m_whiteUV = whiteUV;
}

void UIDrawList::setTransform(const Matrix3& transform) {
	m_transform = transform;
}

Vector2 UIDrawList::toScreen(Vector2 local) const {
	const Vector3 screen = m_transform * Vector3(local, 1.0f);
	return Vector2(screen.x, screen.y);
}

void UIDrawList::pushClip(const UIRect& localRect) {
	// Axis-aligned bounds of the transformed rect (exact without rotation).
	const Vector2 corners[4] = {
		toScreen(localRect.position),
		toScreen(localRect.position + Vector2(localRect.size.x, 0.0f)),
		toScreen(localRect.position + Vector2(0.0f, localRect.size.y)),
		toScreen(localRect.max())};
	Vector2 low = corners[0], high = corners[0];
	for (const Vector2& corner : corners) {
		low = glm::min(low, corner);
		high = glm::max(high, corner);
	}
	m_clipStack.push_back(currentClip().intersect({low, high - low}));
}

void UIDrawList::popClip() {
	if (m_clipStack.size() <= 1) {
		throw std::logic_error("UIDrawList::popClip without matching push");
	}
	m_clipStack.pop_back();
}

const UIRect& UIDrawList::currentClip() const {
	return m_clipStack.back();
}

void UIDrawList::emitQuad(const UIRect& localRect, Vector2 uvMin,
						  Vector2 uvMax, const Vector4& color) {
	const UIRect& clip = currentClip();
	if (clip.size.x <= 0.0f || clip.size.y <= 0.0f || color.a <= 0.0f) {
		return;	 // fully clipped or invisible: emit nothing
	}
	const Vector4 clipBounds(clip.position, clip.max());
	const auto base = static_cast<uint32_t>(m_vertices.size());
	const Vector2 corners[4] = {
		localRect.position, localRect.position + Vector2(0.0f, localRect.size.y),
		localRect.position + Vector2(localRect.size.x, 0.0f), localRect.max()};
	const Vector2 uvs[4] = {uvMin, Vector2(uvMin.x, uvMax.y),
							Vector2(uvMax.x, uvMin.y), uvMax};
	for (int i = 0; i < 4; i++) {
		m_vertices.push_back({toScreen(corners[i]), uvs[i], color, clipBounds});
	}
	m_indices.insert(m_indices.end(),
					 {base, base + 1, base + 2, base + 2, base + 1, base + 3});
}

void UIDrawList::rect(const UIRect& localRect, const Vector4& color) {
	emitQuad(localRect, m_whiteUV, m_whiteUV, color);
}

void UIDrawList::border(const UIRect& r, float t, const Vector4& color) {
	if (t <= 0.0f) return;
	const Vector2 p = r.position, s = r.size;
	rect({p, Vector2(s.x, t)}, color);							  // top
	rect({p + Vector2(0.0f, s.y - t), Vector2(s.x, t)}, color);	  // bottom
	rect({p + Vector2(0.0f, t), Vector2(t, s.y - 2 * t)}, color);  // left
	rect({p + Vector2(s.x - t, t), Vector2(t, s.y - 2 * t)}, color);  // right
}

void UIDrawList::quad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
					  const Vector4& color) {
	emitQuad(localRect, uvMin, uvMax, color);
}

void UIDrawList::text(const Font& font, const std::string& text,
					  Vector2 localPosition, float pixelHeight,
					  const Vector4& color) {
	for (const GlyphQuad& glyph : font.layoutLine(text, pixelHeight)) {
		emitQuad({localPosition + glyph.position, glyph.size}, glyph.uvMin,
				 glyph.uvMax, color);
	}
}

}  // namespace bird
