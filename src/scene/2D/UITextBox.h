#ifndef BIRD_UITEXTBOX_H
#define BIRD_UITEXTBOX_H

#include <functional>
#include <string>

#include "UIObject.h"
#include "UITextEditState.h"

namespace bird {

/**
 * Single-line text input.
 *
 * Mouse: click places the caret, drag or Shift+click selects, double click
 * selects a word. Keys: arrows (Ctrl for words), Home/End, Backspace/
 * Delete (Ctrl for words), Ctrl+A/C/X/V, Enter submits. While focused it
 * owns the keyboard (game actions are suppressed). ASCII only.
 */
class UITextBox : public UIObject {
   public:
	explicit UITextBox(std::string text = "");

	UITextBox& setText(std::string text);
	const std::string& getText() const { return m_state.text(); }
	UITextBox& setPlaceholder(std::string placeholder);
	UITextBox& setMaxLength(size_t maxLength);
	/** Called after every edit with the new text. */
	UITextBox& setOnChanged(std::function<void(const std::string&)> onChanged);
	/** Called when Enter is pressed. */
	UITextBox& setOnSubmit(std::function<void(const std::string&)> onSubmit);
	const UITextEditState& getEditState() const { return m_state; }

	/** @return the caret index closest to a local x coordinate. */
	size_t caretIndexAt(float localX) const;

	bool isFocusable() const override { return true; }
	bool capturesKeyboard() const override { return true; }
	UICursor cursor() const override { return UICursor::IBeam; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	void onFocus(bool focused) override;
	void update(float delta) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	/** Shows the caret, scrolls it into view and notifies listeners. */
	void afterEdit(bool changed);
	float fontSize() const { return theme().fontSize; }

	UITextEditState m_state;
	std::string m_placeholder;
	std::function<void(const std::string&)> m_onChanged;
	std::function<void(const std::string&)> m_onSubmit;
	float m_scrollX = 0.0f;	 // horizontal text scroll inside the box
	float m_blinkTime = 0.0f;
	bool m_caretVisible = true;
};

}  // namespace bird

#endif	// !BIRD_UITEXTBOX_H
