#include "UIDrawList.h"

#include <algorithm>
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

bool UIDrawList::clipIsEmpty() const {
	const UIRect& clip = currentClip();
	return clip.size.x <= 0.0f || clip.size.y <= 0.0f;
}

void UIDrawList::emit(const QuadSpec& spec, const Vector4& color) {
	const UIRect& clip = currentClip();
	const Vector4 clipBounds(clip.position, clip.max());
	const auto base = static_cast<uint32_t>(m_vertices.size());
	for (int i = 0; i < 4; i++) {
		m_vertices.push_back(
			{toScreen(spec.corners[i]), spec.uvs[i], color, clipBounds,
			 Vector4(spec.shapeLocal[i], spec.halfSize), spec.style,
			 spec.borderColor});
	}
	m_indices.insert(m_indices.end(),
					 {base, base + 1, base + 2, base + 2, base + 1, base + 3});
}

void UIDrawList::plainQuad(const UIRect& r, Vector2 uvMin, Vector2 uvMax,
						   const Vector4& color) {
	if (clipIsEmpty() || color.a <= 0.0f) return;
	QuadSpec spec;
	spec.corners[0] = r.position;
	spec.corners[1] = r.position + Vector2(0.0f, r.size.y);
	spec.corners[2] = r.position + Vector2(r.size.x, 0.0f);
	spec.corners[3] = r.max();
	spec.uvs[0] = uvMin;
	spec.uvs[1] = Vector2(uvMin.x, uvMax.y);
	spec.uvs[2] = Vector2(uvMax.x, uvMin.y);
	spec.uvs[3] = uvMax;
	emit(spec, color);
}

void UIDrawList::rect(const UIRect& localRect, const Vector4& color) {
	plainQuad(localRect, m_whiteUV, m_whiteUV, color);
}

void UIDrawList::border(const UIRect& r, float t, const Vector4& color) {
	if (t <= 0.0f) return;
	const Vector2 p = r.position, s = r.size;
	rect({p, Vector2(s.x, t)}, color);
	rect({p + Vector2(0.0f, s.y - t), Vector2(s.x, t)}, color);
	rect({p + Vector2(0.0f, t), Vector2(t, s.y - 2 * t)}, color);
	rect({p + Vector2(s.x - t, t), Vector2(t, s.y - 2 * t)}, color);
}

void UIDrawList::roundedRect(const UIRect& r, const Vector4& fill,
							 float radius, float borderWidth,
							 const Vector4& borderColor) {
	const bool hasBorder = borderWidth > 0.0f && borderColor.a > 0.0f;
	if (clipIsEmpty() || (fill.a <= 0.0f && !hasBorder)) return;
	if (r.size.x <= 0.0f || r.size.y <= 0.0f) return;
	const Vector2 half = r.size * 0.5f;
	const Vector2 center = r.position + half;
	QuadSpec spec;
	spec.corners[0] = r.position;
	spec.corners[1] = r.position + Vector2(0.0f, r.size.y);
	spec.corners[2] = r.position + Vector2(r.size.x, 0.0f);
	spec.corners[3] = r.max();
	for (int i = 0; i < 4; i++) {
		spec.uvs[i] = m_whiteUV;
		spec.shapeLocal[i] = spec.corners[i] - center;
	}
	spec.halfSize = half;
	spec.style = Vector4(std::clamp(radius, 0.0f, std::min(half.x, half.y)),
						 hasBorder ? borderWidth : 0.0f, 0.0f, 0.0f);
	spec.borderColor = borderColor;
	emit(spec, fill);
}

void UIDrawList::shadow(const UIRect& r, float radius, float blur,
						const Vector4& color) {
	if (clipIsEmpty() || color.a <= 0.0f || blur <= 0.0f) return;
	const Vector2 half = r.size * 0.5f;
	const Vector2 center = r.position + half;
	const Vector2 low = r.position - Vector2(blur);
	const Vector2 high = r.max() + Vector2(blur);
	QuadSpec spec;
	spec.corners[0] = low;
	spec.corners[1] = Vector2(low.x, high.y);
	spec.corners[2] = Vector2(high.x, low.y);
	spec.corners[3] = high;
	for (int i = 0; i < 4; i++) {
		spec.uvs[i] = m_whiteUV;
		spec.shapeLocal[i] = spec.corners[i] - center;
	}
	spec.halfSize = glm::max(half, Vector2(0.5f));
	spec.style = Vector4(std::clamp(radius, 0.0f, std::min(half.x, half.y)),
						 0.0f, blur, 0.0f);
	emit(spec, color);
}

void UIDrawList::triangle(Vector2 a, Vector2 b, Vector2 c,
						  const Vector4& color) {
	if (clipIsEmpty() || color.a <= 0.0f) return;
	const UIRect& clip = currentClip();
	const Vector4 clipBounds(clip.position, clip.max());
	const auto base = static_cast<uint32_t>(m_vertices.size());
	for (const Vector2& p : {a, b, c}) {
		m_vertices.push_back({toScreen(p), m_whiteUV, color, clipBounds,
							  Vector4(0.0f), Vector4(0.0f), Vector4(0.0f)});
	}
	m_indices.insert(m_indices.end(), {base, base + 1, base + 2});
}

void UIDrawList::line(Vector2 a, Vector2 b, float thickness,
					  const Vector4& color) {
	if (clipIsEmpty() || color.a <= 0.0f || thickness <= 0.0f) return;
	const Vector2 delta = b - a;
	const float length = glm::length(delta);
	if (length <= 0.0f) return;
	// A capsule: a rounded box in the segment's own frame, radius t/2,
	// padded by one pixel so the anti-aliased edge is not cut off.
	const Vector2 along = delta / length;
	const Vector2 across(-along.y, along.x);
	const float halfThick = thickness * 0.5f;
	const Vector2 half(length * 0.5f + halfThick, halfThick);
	const Vector2 pad = half + Vector2(1.0f);
	const Vector2 center = (a + b) * 0.5f;
	const Vector2 frame[4] = {Vector2(-pad.x, -pad.y), Vector2(-pad.x, pad.y),
							  Vector2(pad.x, -pad.y), Vector2(pad.x, pad.y)};
	QuadSpec spec;
	for (int i = 0; i < 4; i++) {
		spec.corners[i] = center + along * frame[i].x + across * frame[i].y;
		spec.uvs[i] = m_whiteUV;
		spec.shapeLocal[i] = frame[i];
	}
	spec.halfSize = half;
	spec.style = Vector4(halfThick, 0.0f, 0.0f, 0.0f);
	emit(spec, color);
}

void UIDrawList::quad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
					  const Vector4& color) {
	plainQuad(localRect, uvMin, uvMax, color);
}

void UIDrawList::text(const Font& font, const std::string& text,
					  Vector2 localPosition, float pixelHeight,
					  const Vector4& color) {
	for (const GlyphQuad& glyph : font.layoutLine(text, pixelHeight)) {
		plainQuad({localPosition + glyph.position, glyph.size}, glyph.uvMin,
				  glyph.uvMax, color);
	}
}

}  // namespace bird
