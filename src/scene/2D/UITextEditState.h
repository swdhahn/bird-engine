#ifndef BIRD_UITEXTEDITSTATE_H
#define BIRD_UITEXTEDITSTATE_H

#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace bird {

/**
 * Editing model behind UITextBox, free of rendering and input concerns.
 *
 * Positions are byte indices between characters (0..text().size()). The
 * selection spans anchor() to caret(); it is empty when they are equal.
 * Every edit replaces the selection when one exists.
 */
class UITextEditState {
   public:
	explicit UITextEditState(
		size_t maxLength = std::numeric_limits<size_t>::max());

	const std::string& text() const { return m_text; }
	/** Replaces the text (truncated to maxLength), caret at the end. */
	void setText(std::string text);
	void setMaxLength(size_t maxLength);
	size_t maxLength() const { return m_maxLength; }

	size_t caret() const { return m_caret; }
	size_t anchor() const { return m_anchor; }
	bool hasSelection() const { return m_caret != m_anchor; }
	/** @return {start, end} of the selection, start <= end. */
	std::pair<size_t, size_t> selection() const;
	std::string selectedText() const;

	/**
	 * Inserts text at the caret, replacing any selection; truncates to the
	 * remaining capacity. @return true when the text changed.
	 */
	bool insert(const std::string& text);
	/** Deletes the selection, else the character (or word) before caret. */
	bool backspace(bool word = false);
	/** Deletes the selection, else the character (or word) after caret. */
	bool deleteForward(bool word = false);
	/** Deletes the selection. @return true when something was removed. */
	bool eraseSelection();

	void moveLeft(bool select, bool word = false);
	void moveRight(bool select, bool word = false);
	void moveHome(bool select);
	void moveEnd(bool select);
	void selectAll();
	/** Places the caret; with select, extends the selection instead. */
	void setCaret(size_t position, bool select);
	/** Selects the word (run of non-space characters) around a position. */
	void selectWordAt(size_t position);

   private:
	size_t wordLeft(size_t from) const;
	size_t wordRight(size_t from) const;
	void eraseRange(size_t start, size_t end);

	std::string m_text;
	size_t m_caret = 0;
	size_t m_anchor = 0;
	size_t m_maxLength;
};

}  // namespace bird

#endif	// !BIRD_UITEXTEDITSTATE_H
