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
	Vector4 color;	   // RGBA
	Vector4 clip;	   // screen-space clip rect: minX, minY, maxX, maxY
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

	void rect(const UIRect& localRect, const Vector4& color);
	/** Draws a rectangle outline of the given thickness, inside the rect. */
	void border(const UIRect& localRect, float thickness, const Vector4& color);
	/** Draws an atlas-textured quad (glyphs). */
	void quad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
			  const Vector4& color);
	/**
	 * Draws one line of text with its top-left at localPosition.
	 * @param pixelHeight - font size in pixels.
	 */
	void text(const Font& font, const std::string& text, Vector2 localPosition,
			  float pixelHeight, const Vector4& color);

	const std::vector<UIVertex>& vertices() const { return m_vertices; }
	const std::vector<uint32_t>& indices() const { return m_indices; }

   private:
	Vector2 toScreen(Vector2 local) const;
	void emitQuad(const UIRect& localRect, Vector2 uvMin, Vector2 uvMax,
				  const Vector4& color);

	std::vector<UIVertex> m_vertices;
	std::vector<uint32_t> m_indices;
	std::vector<UIRect> m_clipStack;
	Matrix3 m_transform{1.0f};
	Vector2 m_whiteUV;
};

}  // namespace bird

#endif	// !BIRD_UIDRAWLIST_H
