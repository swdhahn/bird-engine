#ifndef BIRD_BUTTON_H
#define BIRD_BUTTON_H

#include <functional>
#include <string>

#include "UIObject.h"

namespace bird {

/**
 * A push button with a centred caption. Activates on a click that is
 * released inside it, or on Enter/Space while focused.
 */
class UIButton : public UIObject {
   public:
	explicit UIButton(std::string text = "");

	UIButton& setText(std::string text);
	const std::string& getText() const { return m_text; }
	/** Sets the callback fired when the button activates. */
	UIButton& setOnClick(std::function<void()> onClick);
	/** Activates the button as if clicked (no effect when disabled). */
	void click();

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;
	/** Fill colour for the current state; tab buttons override it. */
	virtual Vector4 backgroundColor() const;

   private:
	std::string m_text;
	std::function<void()> m_onClick;
};

}  // namespace bird

#endif	// !BIRD_BUTTON_H
