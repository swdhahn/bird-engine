#include "UITextArea.h"

#include <algorithm>
#include <cmath>

#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kDefaultWidth = 240.0f;
constexpr float kCaretWidth = 1.0f;
constexpr float kScrollbarWidth = 6.0f;
constexpr int kWheelLines = 3;
}  // namespace

UITextArea::UITextArea(std::string text) {
	m_interactive = true;
	m_state.setText(std::move(text));
	setPadding(UIInsets::all(theme().padding * 0.75f));
}

UITextArea& UITextArea::setText(std::string text) {
	m_state.setText(std::move(text));
	afterEdit(true);
	return *this;
}

UITextArea& UITextArea::setPlaceholder(std::string placeholder) {
	m_placeholder = std::move(placeholder);
	markVisualDirty();
	return *this;
}

UITextArea& UITextArea::setMaxLength(size_t maxLength) {
	m_state.setMaxLength(maxLength);
	afterEdit(true);
	return *this;
}

UITextArea& UITextArea::setVisibleLines(int lines) {
	m_visibleLines = std::max(lines, 1);
	markLayoutDirty();
	return *this;
}

UITextArea& UITextArea::setOnChanged(
	std::function<void(const std::string&)> onChanged) {
	m_onChanged = std::move(onChanged);
	return *this;
}

UITextArea& UITextArea::setOnSubmit(
	std::function<void(const std::string&)> onSubmit) {
	m_onSubmit = std::move(onSubmit);
	return *this;
}

float UITextArea::lineHeight() const {
	return font() ? font()->getLineHeight(fontSize()) : fontSize();
}

float UITextArea::textWidth() const {
	// The scrollbar's lane is always reserved so wrapping never flickers.
	return std::max(contentSize().x - kScrollbarWidth, 1.0f);
}

void UITextArea::ensureLayout() {
	const float width = textWidth();
	if (m_layoutDirty || width != m_layoutWidth) {
		m_layout.build(font(), m_state.text(), fontSize(), width);
		m_layoutWidth = width;
		m_layoutDirty = false;
	}
}

const UITextLayout& UITextArea::getLayout() {
	ensureLayout();
	return m_layout;
}

float UITextArea::getMaxScroll() {
	ensureLayout();
	const float total = static_cast<float>(m_layout.lineCount()) * lineHeight();
	return std::max(total - contentSize().y, 0.0f);
}

Vector2 UITextArea::measureContent(Vector2 /*available*/) {
	return Vector2(kDefaultWidth,
				   static_cast<float>(m_visibleLines) * lineHeight());
}

size_t UITextArea::caretIndexAt(Vector2 local) {
	ensureLayout();
	const float y = local.y - getPadding().top + m_scrollY;
	const long line = static_cast<long>(std::floor(y / lineHeight()));
	const long clamped =
		std::clamp(line, 0L, static_cast<long>(m_layout.lineCount()) - 1);
	return m_layout.caretAt(static_cast<size_t>(clamped),
							local.x - getPadding().left);
}

void UITextArea::scrollToCaret() {
	ensureLayout();
	const float lh = lineHeight();
	const float top =
		static_cast<float>(m_layout.lineOf(m_state.caret())) * lh;
	const float viewport = contentSize().y;
	if (top < m_scrollY) m_scrollY = top;
	if (top + lh > m_scrollY + viewport) m_scrollY = top + lh - viewport;
	m_scrollY = std::clamp(m_scrollY, 0.0f, getMaxScroll());
}

void UITextArea::afterEdit(bool changed, bool keepPreferredX) {
	if (changed) m_layoutDirty = true;
	ensureLayout();
	if (!keepPreferredX) m_preferredX = m_layout.caretX(m_state.caret());
	m_blinkTime = 0.0f;
	m_caretVisible = true;
	scrollToCaret();
	markVisualDirty();
	if (changed && m_onChanged) m_onChanged(m_state.text());
}

void UITextArea::moveCaretVertical(int lines, bool select) {
	ensureLayout();
	m_state.setCaret(m_layout.moveVertical(m_state.caret(), lines, m_preferredX),
					 select);
	afterEdit(false, true);
}

bool UITextArea::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left) return false;
	const size_t index = caretIndexAt(toLocal(event.position));
	if (event.clickCount >= 2) {
		m_state.selectWordAt(index);
	} else {
		m_state.setCaret(index, event.modifiers & UI_MOD_SHIFT);
	}
	afterEdit(false);
	return true;
}

bool UITextArea::onMouseMove(const UIMouseEvent& event) {
	if (!isPressed()) return false;
	m_state.setCaret(caretIndexAt(toLocal(event.position)), true);
	afterEdit(false);
	return true;
}

bool UITextArea::onMouseUp(const UIMouseEvent& /*event*/) {
	return true;
}

bool UITextArea::onScroll(const UIScrollEvent& event) {
	const float before = m_scrollY;
	m_scrollY = std::clamp(
		m_scrollY - event.delta.y * kWheelLines * lineHeight(), 0.0f,
		getMaxScroll());
	markVisualDirty();
	return m_scrollY != before;	 // at a limit, let outer views scroll
}

bool UITextArea::onKey(const UIKeyEvent& event) {
	const bool shift = event.shift();
	const bool control = event.control();
	ensureLayout();
	switch (event.key) {
		case UIKey::Left:
			m_state.moveLeft(shift, control);
			afterEdit(false);
			return true;
		case UIKey::Right:
			m_state.moveRight(shift, control);
			afterEdit(false);
			return true;
		case UIKey::Up:
			moveCaretVertical(-1, shift);
			return true;
		case UIKey::Down:
			moveCaretVertical(+1, shift);
			return true;
		case UIKey::PageUp:
		case UIKey::PageDown: {
			const int page = std::max(
				static_cast<int>(contentSize().y / lineHeight()) - 1, 1);
			moveCaretVertical(event.key == UIKey::PageUp ? -page : page, shift);
			return true;
		}
		case UIKey::Home: {
			const UITextLine& line =
				m_layout.lines()[m_layout.lineOf(m_state.caret())];
			m_state.setCaret(control ? 0 : line.start, shift);
			afterEdit(false);
			return true;
		}
		case UIKey::End: {
			const UITextLine& line =
				m_layout.lines()[m_layout.lineOf(m_state.caret())];
			m_state.setCaret(control ? m_state.text().size() : line.end, shift);
			afterEdit(false);
			return true;
		}
		case UIKey::Backspace:
			afterEdit(m_state.backspace(control));
			return true;
		case UIKey::Delete:
			afterEdit(m_state.deleteForward(control));
			return true;
		case UIKey::Enter:
			if (control) {
				if (m_onSubmit) m_onSubmit(m_state.text());
			} else {
				afterEdit(m_state.insert("\n"));
			}
			return true;
		case UIKey::A:
			if (!control) return false;
			m_state.selectAll();
			afterEdit(false);
			return true;
		case UIKey::C:
		case UIKey::X:
			if (!control) return false;
			if (m_state.hasSelection() && platform()) {
				platform()->setClipboardText(m_state.selectedText());
				if (event.key == UIKey::X) afterEdit(m_state.eraseSelection());
			}
			return true;
		case UIKey::V:
			if (!control) return false;
			if (platform()) {
				std::string pasted;
				// Keep newlines; drop carriage returns; tabs become spaces.
				for (char c : platform()->getClipboardText()) {
					if (c == '\r') continue;
					if (c == '\t') {
						pasted += "    ";
					} else if (c == '\n' || (c >= 32 && c <= 126)) {
						pasted += c;
					}
				}
				afterEdit(m_state.insert(pasted));
			}
			return true;
		default:
			return false;
	}
}

bool UITextArea::onChar(uint32_t codepoint) {
	if (codepoint < 32 || codepoint > 126) return false;
	afterEdit(m_state.insert(std::string(1, static_cast<char>(codepoint))));
	return true;
}

void UITextArea::onFocus(bool /*focused*/) {
	m_blinkTime = 0.0f;
	m_caretVisible = true;
	markVisualDirty();
}

void UITextArea::update(float delta) {
	m_blinkTime += delta;
	const float period = theme().caretBlinkSeconds;
	const bool visible = std::fmod(m_blinkTime, 2.0f * period) < period;
	if (visible != m_caretVisible) {
		m_caretVisible = visible;
		markVisualDirty();
	}
}

void UITextArea::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	list.roundedRect(bounds, t.inputColor, t.controlRadius, t.borderWidth,
					 isFocused() ? t.focusBorderColor : t.borderColor);
	if (!font()) return;
	ensureLayout();

	const UIRect inner{getPadding().start(), contentSize()};
	const float lh = lineHeight();
	list.pushClip(inner);
	if (m_state.text().empty()) {
		if (!m_placeholder.empty()) {
			list.text(*font(), m_placeholder, inner.position, fontSize(),
					  t.mutedTextColor);
		}
	} else {
		const auto& lines = m_layout.lines();
		const auto [selStart, selEnd] = m_state.selection();
		// Only lines inside the viewport are emitted: O(visible lines).
		const size_t first = static_cast<size_t>(m_scrollY / lh);
		for (size_t i = first; i < lines.size(); i++) {
			const float y = inner.position.y + static_cast<float>(i) * lh - m_scrollY;
			if (y > inner.max().y) break;
			const UITextLine& line = lines[i];
			if (isFocused() && selStart < selEnd && selStart <= line.end &&
				selEnd >= line.start) {
				const float x0 = m_layout.xAt(i, std::max(selStart, line.start));
				float x1 = m_layout.xAt(i, std::min(selEnd, line.end));
				if (selEnd > line.end) x1 += 4.0f;	// selected line break
				list.rect({Vector2(inner.position.x + x0, y), Vector2(x1 - x0, lh)},
						  t.selectionColor);
			}
			list.text(*font(), m_state.text().substr(line.start, line.end - line.start),
					  Vector2(inner.position.x, y), fontSize(),
					  isEffectivelyEnabled() ? t.textColor : t.disabledText);
		}
	}
	if (isFocused() && m_caretVisible) {
		const size_t line = m_layout.lineOf(m_state.caret());
		const float y = inner.position.y + static_cast<float>(line) * lh - m_scrollY;
		list.rect({Vector2(inner.position.x + m_layout.caretX(m_state.caret()), y),
				   Vector2(kCaretWidth, lh)},
				  t.caretColor);
	}
	list.popClip();

	const float maxScroll = getMaxScroll();
	if (maxScroll > 0.0f) {
		const float viewport = inner.size.y;
		const float thumbH = std::max(viewport * viewport / (viewport + maxScroll), 16.0f);
		const float thumbY = inner.position.y + (viewport - thumbH) * (m_scrollY / maxScroll);
		list.roundedRect({Vector2(inner.max().x - kScrollbarWidth + 2.0f, thumbY),
						  Vector2(kScrollbarWidth - 2.0f, thumbH)},
						 t.scrollbarColor, (kScrollbarWidth - 2.0f) * 0.5f);
	}
}

}  // namespace bird
