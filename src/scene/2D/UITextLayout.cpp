#include "UITextLayout.h"

#include <algorithm>

#include "../../renders/generic/Font.h"

namespace bird {

void UITextLayout::build(const Font* font, const std::string& text,
						 float pixelHeight, float maxWidth) {
	m_lines.clear();
	m_x.assign(text.size() + 1, 0.0f);

	// Pen positions per paragraph: m_x[i] is the x before character i.
	for (size_t i = 0; i < text.size(); i++) {
		const bool newline = text[i] == '\n';
		const float advance =
			(font && !newline) ? font->advance(text[i], pixelHeight) : 0.0f;
		m_x[i + 1] = newline ? 0.0f : m_x[i] + advance;
	}

	size_t paragraph = 0;
	while (true) {
		size_t paragraphEnd = text.find('\n', paragraph);
		if (paragraphEnd == std::string::npos) paragraphEnd = text.size();

		size_t lineStart = paragraph;
		size_t lastSpace = std::string::npos;  // index of last space seen
		for (size_t i = paragraph; i < paragraphEnd; i++) {
			if (text[i] == ' ') lastSpace = i;
			const float width = m_x[i + 1] - m_x[lineStart];
			if (maxWidth > 0.0f && width > maxWidth && i > lineStart) {
				// An overflowing space stays on this line; otherwise break
				// after the line's last space, else mid-word.
				size_t breakAt = i;
				if (text[i] == ' ') {
					breakAt = i + 1;
				} else if (lastSpace != std::string::npos &&
						   lastSpace >= lineStart && lastSpace < i) {
					breakAt = lastSpace + 1;
				}
				m_lines.push_back(
					{lineStart, breakAt, m_x[breakAt] - m_x[lineStart]});
				lineStart = breakAt;
				lastSpace = std::string::npos;
				i = breakAt - 1;  // re-scan from the new line start
			}
		}
		m_lines.push_back(
			{lineStart, paragraphEnd, m_x[paragraphEnd] - m_x[lineStart]});
		if (paragraphEnd >= text.size()) break;
		paragraph = paragraphEnd + 1;  // skip the '\n'
	}
}

size_t UITextLayout::lineOf(size_t caret) const {
	// Last line starting at or before the caret: O(log lines).
	auto it = std::upper_bound(
		m_lines.begin(), m_lines.end(), caret,
		[](size_t c, const UITextLine& line) { return c < line.start; });
	return it == m_lines.begin() ? 0
								 : static_cast<size_t>(it - m_lines.begin()) - 1;
}

float UITextLayout::caretX(size_t caret) const {
	return m_lines.empty() ? 0.0f : xAt(lineOf(caret), caret);
}

float UITextLayout::xAt(size_t lineIndex, size_t caret) const {
	if (m_lines.empty()) return 0.0f;
	const UITextLine& line = m_lines[std::min(lineIndex, m_lines.size() - 1)];
	caret = std::clamp(caret, line.start, line.end);
	return m_x[caret] - m_x[line.start];
}

size_t UITextLayout::caretAt(size_t lineIndex, float x) const {
	if (m_lines.empty()) return 0;
	const UITextLine& line = m_lines[std::min(lineIndex, m_lines.size() - 1)];
	const float target = m_x[line.start] + x;
	// Boundaries within a line are sorted, so a binary search suffices.
	const auto first = m_x.begin() + static_cast<long>(line.start);
	const auto last = m_x.begin() + static_cast<long>(line.end) + 1;
	size_t upper = static_cast<size_t>(
		std::lower_bound(first, last, target) - m_x.begin());
	if (upper > line.end) return line.end;	// past the end of the line
	if (upper == line.start) return line.start;
	const size_t lower = upper - 1;
	return target - m_x[lower] <= m_x[upper] - target ? lower : upper;
}

size_t UITextLayout::moveVertical(size_t caret, int deltaLines,
								  float preferredX) const {
	if (m_lines.empty()) return 0;
	const long target = static_cast<long>(lineOf(caret)) + deltaLines;
	const long clamped =
		std::clamp(target, 0L, static_cast<long>(m_lines.size()) - 1);
	return caretAt(static_cast<size_t>(clamped), preferredX);
}

float UITextLayout::maxLineWidth() const {
	float width = 0.0f;
	for (const UITextLine& line : m_lines) width = std::max(width, line.width);
	return width;
}

}  // namespace bird
