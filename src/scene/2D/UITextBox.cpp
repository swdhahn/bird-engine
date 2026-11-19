#include "UITextBox.h"

#include <algorithm>
#include <cmath>

#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 200.0f;
constexpr float kCaretWidth = 1.0f;
}  // namespace

UITextBox::UITextBox(std::string text) {
	m_interactive = true;
	m_state.setText(std::move(text));
	setPadding(UIInsets::symmetric(theme().padding, theme().padding * 0.5f));
	UISizeRule height = UISizeRule::fit();
	height.min = theme().controlHeight;
	setHeight(height);
}

UITextBox& UITextBox::setText(std::string text) {
	m_state.setText(std::move(text));
	afterEdit(true);
	return *this;
}

UITextBox& UITextBox::setPlaceholder(std::string placeholder) {
	m_placeholder = std::move(placeholder);
	markVisualDirty();
	return *this;
}

UITextBox& UITextBox::setMaxLength(size_t maxLength) {
	m_state.setMaxLength(maxLength);
	afterEdit(true);
	return *this;
}

UITextBox& UITextBox::setOnChanged(
	std::function<void(const std::string&)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

UITextBox& UITextBox::setOnSubmit(
	std::function<void(const std::string&)> onSubmit) {
	m_onSubmit = std::move(onSubmit);
	return *this;
}

Vector2 UITextBox::measureContent(Vector2 /*available*/) {
	const float lineHeight =
		font() ? font()->getLineHeight(fontSize()) : fontSize();
	return Vector2(kDefaultWidth, lineHeight);
}

size_t UITextBox::caretIndexAt(float localX) const {
	if (!font()) return 0;
	const std::vector<float> offsets =
		font()->caretOffsets(displayText(), fontSize());
	const float textX = localX - getPadding().left + m_scrollX;
	// Nearest boundary: offsets are sorted, so binary search is O(log n).
	auto it = std::lower_bound(offsets.begin(), offsets.end(), textX);
	if (it == offsets.end()) return offsets.size() - 1;
	if (it != offsets.begin() && textX - *(it - 1) < *it - textX) --it;
	return static_cast<size_t>(it - offsets.begin());
}

void UITextBox::afterEdit(bool changed) {
	m_blinkTime = 0.0f;
	m_caretVisible = true;
	if (font()) {
		const float caretX =
			font()->caretOffsets(displayText(), fontSize())[m_state.caret()];
		const float innerWidth = std::max(contentSize().x, 1.0f);
		if (caretX - m_scrollX > innerWidth) m_scrollX = caretX - innerWidth;
		if (caretX < m_scrollX) m_scrollX = caretX;
		m_scrollX = std::max(m_scrollX, 0.0f);
	}
	markVisualDirty();
	if (changed && m_onChanged) m_onChanged(m_state.text());
}

bool UITextBox::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	const size_t index = caretIndexAt(toLocal(event.position).x);
	if (event.clickCount >= 2) {
		m_state.selectWordAt(index);
	} else {
		m_state.setCaret(index, event.modifiers & UI_MOD_SHIFT);
	}
	afterEdit(false);
	return true;
}

bool UITextBox::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	m_state.setCaret(caretIndexAt(toLocal(event.position).x), true);
	afterEdit(false);
	return true;
}

bool UITextBox::onMouseUp(const UIMouseEvent& /*event*/) {
	return true;
}

bool UITextBox::onKey(const UIKeyEvent& event) {
	const bool shift = event.shift();
	const bool control = event.control();
	switch (event.key) {
		case UIKey::Left:
			m_state.moveLeft(shift, control);
			afterEdit(false);
			return true;
		case UIKey::Right:
			m_state.moveRight(shift, control);
			afterEdit(false);
			return true;
		case UIKey::Home:
			m_state.moveHome(shift);
			afterEdit(false);
			return true;
		case UIKey::End:
			m_state.moveEnd(shift);
			afterEdit(false);
			return true;
		case UIKey::Backspace:
			afterEdit(m_state.backspace(control));
			return true;
		case UIKey::Delete:
			afterEdit(m_state.deleteForward(control));
			return true;
		case UIKey::Enter:
			if (m_onSubmit) m_onSubmit(m_state.text());
			return true;
		case UIKey::A:
			if (!control) return false;
			m_state.selectAll();
			afterEdit(false);
			return true;
		case UIKey::C:
		case UIKey::X:
			if (!control) return false;
			// Masked text never reaches the clipboard.
			if (m_state.hasSelection() && platform() && !m_password) {
				platform()->setClipboardText(m_state.selectedText());
				if (event.key == UIKey::X) afterEdit(m_state.eraseSelection());
			}
			return true;
		case UIKey::V:
			if (!control) return false;
			if (platform()) {
				std::string pasted = platform()->getClipboardText();
				// Single line: newlines and tabs become spaces.
				std::replace_if(
					pasted.begin(), pasted.end(),
					[](char c) { return c == '\n' || c == '\r' || c == '\t'; },
					' ');
				afterEdit(m_state.insert(pasted));
			}
			return true;
		default:
			return false;
	}
}

std::string UITextBox::displayText() const {
	return m_password ? std::string(m_state.text().size(), kMaskCharacter)
					  : m_state.text();
}

UITextBox& UITextBox::setPassword(bool password) {
	m_password = password;
	afterEdit(false);
	return *this;
}

bool UITextBox::acceptsCharacter(uint32_t codepoint) const {
	// The font atlas is ASCII; other characters and controls are ignored.
	return codepoint >= 32 && codepoint <= 126;
}

bool UITextBox::onChar(uint32_t codepoint) {
	if (!acceptsCharacter(codepoint)) return false;
	afterEdit(m_state.insert(std::string(1, static_cast<char>(codepoint))));
	return true;
}

void UITextBox::onFocus(bool /*focused*/) {
	m_blinkTime = 0.0f;
	m_caretVisible = true;
	markVisualDirty();
}

void UITextBox::update(float delta) {
	m_blinkTime += delta;
	const bool visible =
		std::fmod(m_blinkTime, 2.0f * theme().caretBlinkSeconds) <
		theme().caretBlinkSeconds;
	if (visible != m_caretVisible) {
		m_caretVisible = visible;
		markVisualDirty();
	}
}

void UITextBox::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.roundedRect(bounds, t.inputColor, t.controlRadius, t.borderWidth,
					 isFocused() ? t.focusBorderColor : t.borderColor);
	if (!font()) return;

	const UIRect inner{getPadding().start(), contentSize()};
	list.pushClip(inner);
	const float textX = inner.position.x - m_scrollX;
	const float lineHeight = font()->getLineHeight(fontSize());
	const float textY = inner.position.y + (inner.size.y - lineHeight) * 0.5f;

	if (m_state.text().empty()) {
		if (!m_placeholder.empty()) {
			list.text(*font(), m_placeholder, Vector2(inner.position.x, textY),
					  fontSize(), t.mutedTextColor);
		}
	} else {
		const std::vector<float> offsets =
			font()->caretOffsets(displayText(), fontSize());
		if (isFocused() && m_state.hasSelection()) {
			const auto [start, end] = m_state.selection();
			list.rect({Vector2(textX + offsets[start], textY),
					   Vector2(offsets[end] - offsets[start], lineHeight)},
					  t.selectionColor);
		}
		list.text(*font(), displayText(), Vector2(textX, textY), fontSize(),
				  isEffectivelyEnabled() ? t.textColor : t.disabledText);
	}
	if (isFocused() && m_caretVisible) {
		const float caretX =
			font()->caretOffsets(displayText(), fontSize())[m_state.caret()];
		list.rect({Vector2(textX + caretX, textY),
				   Vector2(kCaretWidth, lineHeight)},
				  t.caretColor);
	}
	list.popClip();
}

}  // namespace bird
