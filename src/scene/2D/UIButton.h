#ifndef BIRD_BUTTON_H
#define BIRD_BUTTON_H

#include <functional>
#include <string>

#include "UIObject.h"
namespace bird {

class UILabel;

/**
 * A rectangular button made of a border quad and a fill quad. The fill
 * colour follows the theme's surface, hover and accent colours.
 */
class UIButton : public UIObject {
   public:
	UIButton();
	~UIButton();

	void onPress(float mx, float my) override;
	void onRelease(float mx, float my) override;
	void onHover(bool hovered) override;
	/** Keeps the caption centred within the button. */
	void layout() override;

	/** Sets the centred caption, creating its label on first use. */
	void setText(const std::string& text);

	/** Sets the callback fired when a press is released inside the button. */
	void setOnClick(std::function<void()> onClick);

   private:
	void refreshFillColor();

	std::function<void()> m_onClick;
	UILabel* m_label = nullptr;	 // owned by m_children
	bool m_hovered = false;
	bool m_pressed = false;
};

}  // namespace bird

#endif	// !BIRD_BUTTON_H
