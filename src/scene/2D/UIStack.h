#ifndef BIRD_UISTACK_H
#define BIRD_UISTACK_H

#include <optional>

#include "UIObject.h"

namespace bird {

/**
 * Lays children out in a row (Horizontal) or column (Vertical).
 *
 * - gap: pixels between consecutive visible children (default: theme
 *   itemSpacing).
 * - Fill children share the remaining main-axis space by weight; otherwise
 *   justify distributes the free space (Start, Center, End, SpaceBetween).
 * - align places children on the cross axis; Stretch (or a child with a
 *   Fill cross rule) makes them span it.
 *
 * Anchors inside a stack (Flow children): the stack owns the main axis.
 * On the cross axis, explicitly set anchors override align: unequal
 * anchors stretch the child between those fractions of the cross space,
 * an equal anchor places it at anchor * space - pivot * size. The child's
 * offset nudges it visually without moving its siblings.
 *
 * Absolute children leave the flow: they take no space and are placed by
 * their anchors inside the content box (badges, overlays). Margins are
 * honoured in both modes.
 */
class UIStack : public UIObject {
   public:
	explicit UIStack(UIAxis axis = UIAxis::Vertical);

	UIStack& setAxis(UIAxis axis);
	UIStack& setGap(float gap);
	UIStack& setJustify(UIJustify justify);
	UIStack& setAlign(UIAlign align);

	UIAxis getAxis() const { return m_axis; }
	float getGap() const;

   protected:
	Vector2 measureContent(Vector2 available) override;
	void arrangeChildren() override;

   private:
	UIAxis m_axis;
	std::optional<float> m_gap;
	UIJustify m_justify = UIJustify::Start;
	UIAlign m_align = UIAlign::Start;
};

}  // namespace bird

#endif	// !BIRD_UISTACK_H
