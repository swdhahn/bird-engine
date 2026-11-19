#ifndef BIRD_UICHECKBOX_H
#define BIRD_UICHECKBOX_H

#include <functional>
#include <string>

#include "UIObject.h"

namespace bird {

/** A box with a caption; click or Space/Enter toggles it. */
class UICheckBox : public UIObject {
   public:
	explicit UICheckBox(std::string text = "", bool checked = false);

	UICheckBox& setChecked(bool checked);
	bool isChecked() const { return m_checked; }
	UICheckBox& setText(std::string text);
	/** Called with the new state after every toggle. */
	UICheckBox& setOnChanged(std::function<void(bool)> onChanged);
	void toggle();

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
	bool m_checked;
	std::function<void(bool)> m_onChanged;
};

}  // namespace bird

#endif	// !BIRD_UICHECKBOX_H
