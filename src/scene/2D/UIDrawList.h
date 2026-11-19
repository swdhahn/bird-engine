#ifndef BIRD_UIDRAWLIST_H
#define BIRD_UIDRAWLIST_H

#include <cstdint>
#include <string>
#include <vector>

#include "../../util/Math.h"
#include "UILayout.h"

namespace bird {

class Font;

/** Interleaved vertex consumed by the UI shader (one draw call). */
struct UIVertex {
	Vector2 position;  // window coordinates
	Vector2 uv;		   // font atlas; solid quads use its white texel
	Vector4 color;	   // RGBA fill
	Vector4 clip;	   // screen-space clip rect: minX, minY, maxX, maxY
	// Rounded-box shape, evaluated per fragment as a signed distance:
	// xy = position relative to the box centre, zw = box half size.
	// A zero half size disables the shape (glyphs, triangles, plain rects).
	Vector4 shape;
	Vector4 style;		  // x = corner radius, y = border width, z = softness
	Vector4 borderColor;  // RGBA, blended in over the inner border band
};

/**
 * CPU-side list of UI triangles. Primitives are given in the current
 * element's local space (see setTransform) and clipped against the current
 * clip rect in the fragment shader. Independent of any graphics API.
 */
class UIDrawList {
   public:
	/** @param whiteUV - atlas texel that is fully covered (solid colour). */
	explicit UIDrawList(Vector2 whiteUV = Vector2(0.0f));

	void clear();
	void setWhiteUV(Vector2 whiteUV);

	/** Local-to-screen transform for subsequent primitives. */
	void setTransform(const Matrix3& transform);
	/** Intersects a local-space rect with the current clip and pushes it. */
	void pushClip(const UIRect& localRect);
	void popClip();
	/** @return the active clip rect in screen space. */
	const UIRect& currentClip() const;

	/** Square, crisp rectangle. */
	void rect(const UIRect& localRect, const Vector4& color);
	/** Square outline of the given thickness, inside the rect. */
	void border(const UIRect& localRect, float thickness, const Vector4& color);
	/**
	 * Anti-aliased rounded rectangle with an optional inner border.
	 * @param radius - corner radius, clamped to half the shorter side.
	 */
	void roundedRect(const UIRect& localRect, const Vector4& fill, float radius,
					 float borderWidth = 0.0f,
					 const Vector4& borderColor = Vector4(0.0f));
	/** Soft drop shadow: the rect's rounded shape blurred by `blur` px. */
	void shadow(const UIRect& localRect, float radius, float blur,
				const Vector4& color);
	/** Solid triangle (chevrons, arrows). */
	void triangle(Vector2 a, Vector2 b, Vector2 c, const Vector4& color);
	/** Anti-aliased line segment with round caps. */
	void line(Vector2 a, Vector2 b, float thickness, const Vector4& color);
	/** Atlas-textured quad (glyphs). */
	void quad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
			  const Vector4& color);
	/** One line of text with its top-left at localPosition. */
	void text(const Font& font, const std::string& text, Vector2 localPosition,
			  float pixelHeight, const Vector4& color);

	const std::vector<UIVertex>& vertices() const { return m_vertices; }
	const std::vector<uint32_t>& indices() const { return m_indices; }

   private:
	/** Corner order TL, BL, TR, BR in every array. */
	struct QuadSpec {
		Vector2 corners[4];
		Vector2 uvs[4];
		Vector2 shapeLocal[4]{Vector2(0.0f), Vector2(0.0f), Vector2(0.0f),
							  Vector2(0.0f)};
		Vector2 halfSize{0.0f};
		Vector4 style{0.0f};
		Vector4 borderColor{0.0f};
	};

	Vector2 toScreen(Vector2 local) const;
	bool clipIsEmpty() const;
	void emit(const QuadSpec& spec, const Vector4& color);
	void plainQuad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
				   const Vector4& color);

	std::vector<UIVertex> m_vertices;
	std::vector<uint32_t> m_indices;
	std::vector<UIRect> m_clipStack;
	Matrix3 m_transform{1.0f};
	Vector2 m_whiteUV;
};

}  // namespace bird

#endif	// !BIRD_UIDRAWLIST_H
