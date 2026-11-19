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
 * Children's anchors are ignored inside a stack; margins are honoured.
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
