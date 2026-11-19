#ifndef BIRD_UITEXTLAYOUT_H
#define BIRD_UITEXTLAYOUT_H

#include <cstddef>
#include <string>
#include <vector>

namespace bird {

class Font;

/** One visual line: characters [start, end), excluding any '\n'. */
struct UITextLine {
	size_t start;
	size_t end;
	float width;
};

/**
 * Wraps text into visual lines and maps between caret indices and line/x
 * positions. Pure (no rendering or input), so it is unit-tested directly.
 *
 * Wrapping: '\n' always breaks; otherwise a line breaks after the last
 * space that keeps it within maxWidth, or mid-word when a single word is
 * wider than maxWidth. build() is O(n) in the text length.
 *
 * A caret at the boundary between two soft-wrapped lines belongs to the
 * later line (it is drawn at that line's start).
 */
class UITextLayout {
   public:
	/**
	 * @param font - may be nullptr (all advances 0: one line per paragraph).
	 * @param maxWidth - wrap width in pixels; <= 0 disables soft wrapping.
	 */
	void build(const Font* font, const std::string& text, float pixelHeight,
			   float maxWidth);

	const std::vector<UITextLine>& lines() const { return m_lines; }
	size_t lineCount() const { return m_lines.size(); }
	/** Index of the visual line containing a caret position. */
	size_t lineOf(size_t caret) const;
	/** x of a caret within its line, in pixels. */
	float caretX(size_t caret) const;
	/** x of a caret measured on a given line (caret clamped to it). */
	float xAt(size_t lineIndex, size_t caret) const;
	/** Nearest caret on a line to an x coordinate. */
	size_t caretAt(size_t line, float x) const;
	/**
	 * Caret after moving `deltaLines` lines (clamped), keeping preferredX.
	 */
	size_t moveVertical(size_t caret, int deltaLines, float preferredX) const;
	/** Widest line, for measuring. */
	float maxLineWidth() const;

   private:
	std::vector<UITextLine> m_lines;
	// Pen x of every caret boundary, measured from its paragraph start.
	std::vector<float> m_x;
};

}  // namespace bird

#endif	// !BIRD_UITEXTLAYOUT_H
