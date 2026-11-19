#ifndef BIRD_UIPANEL_H
#define BIRD_UIPANEL_H

#include <optional>

#include "UIObject.h"

namespace bird {

/**
 * A filled, bordered container with themed padding. Interactive, so clicks
 * on its background are consumed by the UI instead of reaching the game.
 * Children are placed by anchors (use a UIStack child for flow layout).
 */
class UIPanel : public UIObject {
   public:
	UIPanel();

	/** Overrides the theme's panel colour (alpha 0 hides the fill). */
	UIPanel& setBackground(const Vector4& color);
	UIPanel& setBorder(bool border);
	/** Clips children to the panel's bounds. */
	UIPanel& setClipChildren(bool clip);
	/** Corner radius; defaults to the theme's panelRadius. */
	UIPanel& setCornerRadius(float radius);
	/** Draws a soft drop shadow behind the panel. */
	UIPanel& setShadow(bool shadow);

	void draw(UIDrawList& list) override;

   private:
	std::optional<Vector4> m_background;
	bool m_border = true;
	bool m_shadow = false;
	std::optional<float> m_radius;
};

}  // namespace bird

#endif	// !BIRD_UIPANEL_H
