#ifndef BIRD_UISLIDER_H
#define BIRD_UISLIDER_H

#include <functional>

#include "UIObject.h"

namespace bird {

/**
 * Horizontal value slider. Click jumps, drag follows the pointer (even
 * outside the slider), Left/Right/Up/Down step, Home/End go to the limits.
 */
class UISlider : public UIObject {
   public:
	UISlider(float min = 0.0f, float max = 1.0f, float value = 0.0f);

	/** @throws std::invalid_argument when max <= min. */
	UISlider& setRange(float min, float max);
	/** Snaps values to multiples of step from min; 0 disables snapping. */
	UISlider& setStep(float step);
	/** Clamps and snaps; notifies onChanged when the value changes. */
	UISlider& setValue(float value);
	UISlider& setOnChanged(std::function<void(float)> onChanged);

	float getValue() const { return m_value; }
	float getMin() const { return m_min; }
	float getMax() const { return m_max; }
	/** @return the value as a 0..1 fraction of the range. */
	float getFraction() const;
	/** @return the value under a local x coordinate. */
	float valueAt(float localX) const;

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	float snap(float value) const;
	/** Usable track: thumb centres travel between these local x values. */
	float trackStart() const;
	float trackLength() const;

	float m_min, m_max, m_value;
	float m_step = 0.0f;
	std::function<void(float)> m_onChanged;
};

}  // namespace bird

#endif	// !BIRD_UISLIDER_H
