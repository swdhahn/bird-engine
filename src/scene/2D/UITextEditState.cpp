#include "UITextEditState.h"

#include <algorithm>
#include <cctype>

namespace bird {

namespace {
bool isSpace(char c) {
	return std::isspace(static_cast<unsigned char>(c)) != 0;
}
}  // namespace

UITextEditState::UITextEditState(size_t maxLength) : m_maxLength(maxLength) {}

void UITextEditState::setText(std::string text) {
	if (text.size() > m_maxLength) text.resize(m_maxLength);
	m_text = std::move(text);
	m_caret = m_anchor = m_text.size();
}

void UITextEditState::setMaxLength(size_t maxLength) {
	m_maxLength = maxLength;
	if (m_text.size() > m_maxLength) setText(m_text);
}

std::pair<size_t, size_t> UITextEditState::selection() const {
	return {std::min(m_caret, m_anchor), std::max(m_caret, m_anchor)};
}

std::string UITextEditState::selectedText() const {
	const auto [start, end] = selection();
	return m_text.substr(start, end - start);
}

void UITextEditState::eraseRange(size_t start, size_t end) {
	m_text.erase(start, end - start);
	m_caret = m_anchor = start;
}

bool UITextEditState::eraseSelection() {
	if (!hasSelection()) return false;
	const auto [start, end] = selection();
	eraseRange(start, end);
	return true;
}

bool UITextEditState::insert(const std::string& text) {
	const bool erased = eraseSelection();
	const size_t capacity = m_maxLength - std::min(m_maxLength, m_text.size());
	const std::string accepted = text.substr(0, capacity);
	if (accepted.empty()) return erased;
	m_text.insert(m_caret, accepted);
	m_caret = m_anchor = m_caret + accepted.size();
	return true;
}

bool UITextEditState::backspace(bool word) {
	if (eraseSelection()) return true;
	if (m_caret == 0) return false;
	eraseRange(word ? wordLeft(m_caret) : m_caret - 1, m_caret);
	return true;
}

bool UITextEditState::deleteForward(bool word) {
	if (eraseSelection()) return true;
	if (m_caret >= m_text.size()) return false;
	const size_t end = word ? wordRight(m_caret) : m_caret + 1;
	const size_t start = m_caret;
	eraseRange(start, end);
	return true;
}

void UITextEditState::moveLeft(bool select, bool word) {
	if (!select && hasSelection() && !word) {
		setCaret(selection().first, false);	 // collapse to the left edge
		return;
	}
	setCaret(word ? wordLeft(m_caret) : (m_caret > 0 ? m_caret - 1 : 0),
			 select);
}

void UITextEditState::moveRight(bool select, bool word) {
	if (!select && hasSelection() && !word) {
		setCaret(selection().second, false);  // collapse to the right edge
		return;
	}
	setCaret(word ? wordRight(m_caret)
				  : std::min(m_caret + 1, m_text.size()),
			 select);
}

void UITextEditState::moveHome(bool select) {
	setCaret(0, select);
}

void UITextEditState::moveEnd(bool select) {
	setCaret(m_text.size(), select);
}

void UITextEditState::selectAll() {
	m_anchor = 0;
	m_caret = m_text.size();
}

void UITextEditState::setCaret(size_t position, bool select) {
	m_caret = std::min(position, m_text.size());
	if (!select) m_anchor = m_caret;
}

void UITextEditState::selectWordAt(size_t position) {
	position = std::min(position, m_text.size());
	size_t start = position, end = position;
	while (start > 0 && !isSpace(m_text[start - 1])) start--;
	while (end < m_text.size() && !isSpace(m_text[end])) end++;
	m_anchor = start;
	m_caret = end;
}

size_t UITextEditState::wordLeft(size_t from) const {
	while (from > 0 && isSpace(m_text[from - 1])) from--;
	while (from > 0 && !isSpace(m_text[from - 1])) from--;
	return from;
}

size_t UITextEditState::wordRight(size_t from) const {
	while (from < m_text.size() && isSpace(m_text[from])) from++;
	while (from < m_text.size() && !isSpace(m_text[from])) from++;
	return from;
}

}  // namespace bird
