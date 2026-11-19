#ifndef BIRD_UITEXTAREA_H
#define BIRD_UITEXTAREA_H

#include <functional>
#include <string>

#include "UIObject.h"
#include "UITextEditState.h"
#include "UITextLayout.h"

namespace bird {

/**
 * Multi-line text input with word wrap and vertical scrolling.
 *
 * Keys: arrows (Ctrl for words), Up/Down keep the caret's column,
 * PageUp/PageDown move by a page, Home/End per line (Ctrl: whole text),
 * Enter inserts a newline, Ctrl+Enter submits, Ctrl+A/C/X/V. Mouse: click,
 * drag and Shift+click select, double click selects a word, wheel scrolls.
 * Only visible lines are drawn. ASCII only.
 */
class UITextArea : public UIObject {
   public:
	explicit UITextArea(std::string text = "");

	UITextArea& setText(std::string text);
	const std::string& getText() const { return m_state.text(); }
	UITextArea& setPlaceholder(std::string placeholder);
	UITextArea& setMaxLength(size_t maxLength);
	/** Natural height in lines (the area scrolls beyond it). */
	UITextArea& setVisibleLines(int lines);
	UITextArea& setOnChanged(std::function<void(const std::string&)> onChanged);
	/** Called on Ctrl+Enter. */
	UITextArea& setOnSubmit(std::function<void(const std::string&)> onSubmit);

	const UITextEditState& getEditState() const { return m_state; }
	/** Wrapped lines for the current width (rebuilt on demand). */
	const UITextLayout& getLayout();
	float getScrollOffset() const { return m_scrollY; }
	float getMaxScroll();
	float lineHeight() const;
	/** Caret index nearest to a local point. */
	size_t caretIndexAt(Vector2 local);

	bool isFocusable() const override { return true; }
	bool capturesKeyboard() const override { return true; }
	UICursor cursor() const override { return UICursor::IBeam; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onScroll(const UIScrollEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	bool onChar(uint32_t codepoint) override;
	void onFocus(bool focused) override;
	void update(float delta) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	float fontSize() const { return theme().fontSize; }
	float textWidth() const;
	/** Rebuilds the layout when the text or width changed. */
	void ensureLayout();
	/** Moves the caret, optionally selecting, without touching preferredX. */
	void moveCaretVertical(int lines, bool select);
	void afterEdit(bool changed, bool keepPreferredX = false);
	void scrollToCaret();

	UITextEditState m_state;
	UITextLayout m_layout;
	bool m_layoutDirty = true;
	float m_layoutWidth = -1.0f;
	std::string m_placeholder;
	std::function<void(const std::string&)> m_onChanged;
	std::function<void(const std::string&)> m_onSubmit;
	int m_visibleLines = 5;
	float m_scrollY = 0.0f;
	float m_preferredX = 0.0f;
	float m_blinkTime = 0.0f;
	bool m_caretVisible = true;
};

}  // namespace bird

#endif	// !BIRD_UITEXTAREA_H
