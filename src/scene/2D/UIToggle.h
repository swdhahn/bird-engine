#ifndef BIRD_UITOGGLE_H
#define BIRD_UITOGGLE_H

#include <functional>
#include <string>

#include "UIAnimator.h"
#include "UIObject.h"

namespace bird {

/**
 * On/off switch: a pill track with a sliding knob and a caption. Click or
 * Space/Enter toggles it; the knob animates over UITheme::animationSeconds.
 */
class UIToggle : public UIObject {
   public:
	explicit UIToggle(std::string text = "", bool on = false);

	UIToggle& setOn(bool on);
	bool isOn() const { return m_on; }
	UIToggle& setText(std::string text);
	UIToggle& setOnChanged(std::function<void(bool)> onChanged);
	void toggle();
	/** Knob position, 0 (off) to 1 (on); moves towards isOn() over time. */
	float getKnobPosition() const { return m_knob; }

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	std::string m_text;
	bool m_on;
	float m_knob;
	/** Running knob transition, superseded on every toggle. */
	AnimationHandle m_knobAnimation;
	std::function<void(bool)> m_onChanged;
};

}  // namespace bird

#endif	// !BIRD_UITOGGLE_H
