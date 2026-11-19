#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UITextArea.h"
#include "scene/2D/UITextLayout.h"

using namespace bird;
using bird::test::UIHarness;

namespace {

/** Fake font at 14px: every glyph advances 7px, lines are 14px tall. */
UITextLayout layoutOf(const std::string& text, float maxWidth) {
	static const auto font = bird::test::makeFakeFont();
	UITextLayout layout;
	layout.build(font.get(), text, 14.0f, maxWidth);
	return layout;
}

void expectLine(const UITextLine& line, size_t start, size_t end) {
	EXPECT_EQ(line.start, start);
	EXPECT_EQ(line.end, end);
}

}  // namespace

TEST(UITextLayout, WrapsAfterSpaces) {
	// 7 characters (49px) fit in 50px.
	const UITextLayout layout = layoutOf("aaa bbb ccc", 50.0f);
	ASSERT_EQ(layout.lineCount(), 2u);
	expectLine(layout.lines()[0], 0, 8);  // "aaa bbb " keeps its space
	expectLine(layout.lines()[1], 8, 11);
}

TEST(UITextLayout, NewlinesAlwaysBreak) {
	const UITextLayout layout = layoutOf("ab\ncd\n", 0.0f);
	ASSERT_EQ(layout.lineCount(), 3u);
	expectLine(layout.lines()[0], 0, 2);
	expectLine(layout.lines()[1], 3, 5);
	expectLine(layout.lines()[2], 6, 6);  // empty last line
}

TEST(UITextLayout, LongWordsBreakMidWord) {
	const UITextLayout layout = layoutOf("abcdefghij", 30.0f);	// 4 per line
	ASSERT_EQ(layout.lineCount(), 3u);
	expectLine(layout.lines()[0], 0, 4);
	expectLine(layout.lines()[1], 4, 8);
	expectLine(layout.lines()[2], 8, 10);
}

TEST(UITextLayout, CaretMappingAndVerticalMoves) {
	const UITextLayout layout = layoutOf("aaa bbb ccc", 50.0f);
	EXPECT_EQ(layout.lineOf(8), 1u);  // a wrap boundary belongs below
	EXPECT_FLOAT_EQ(layout.caretX(9), 7.0f);
	EXPECT_EQ(layout.caretAt(1, 100.0f), 11u);	// past the end
	EXPECT_EQ(layout.moveVertical(9, -1, 7.0f), 1u);
	EXPECT_EQ(layout.moveVertical(2, +1, 14.0f), 10u);
	EXPECT_EQ(layout.moveVertical(2, +5, 14.0f), 10u);	// clamped
}

TEST(UITextLayout, EmptyTextHasOneLine) {
	const UITextLayout layout = layoutOf("", 50.0f);
	ASSERT_EQ(layout.lineCount(), 1u);
	EXPECT_EQ(layout.caretAt(0, 30.0f), 0u);
}

TEST(UITextArea, EnterInsertsNewlineCtrlEnterSubmits) {
	UIHarness h;
	auto* area = h.root()->add<UITextArea>();
	std::string submitted;
	area->setOnSubmit([&](const std::string& t) { submitted = t; });
	h.click(area);
	h.type("ab");
	h.press(UIKey::Enter);
	h.type("cd");
	EXPECT_EQ(area->getText(), "ab\ncd");
	h.press(UIKey::Enter, UI_MOD_CONTROL);
	EXPECT_EQ(submitted, "ab\ncd");
}

TEST(UITextArea, UpDownKeepTheColumn) {
	UIHarness h;
	auto* area = h.root()->add<UITextArea>("abcdef\nab\nabcdef");
	h.click(area);
	h.press(UIKey::Home, UI_MOD_CONTROL);
	h.press(UIKey::End);
	EXPECT_EQ(area->getEditState().caret(), 6u);
	h.press(UIKey::Down);
	EXPECT_EQ(area->getEditState().caret(), 9u);  // end of the short line
	h.press(UIKey::Down);
	EXPECT_EQ(area->getEditState().caret(), 16u);	 // column restored
	h.press(UIKey::Up, UI_MOD_SHIFT);
	EXPECT_EQ(area->getEditState().selectedText(), "\nabcdef");
}

TEST(UITextArea, SelectionHighlightsEveryLine) {
	UIHarness h;
	auto* area = h.root()->add<UITextArea>("one\ntwo\nthree");
	h.click(area);
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.frame();
	const Vector4 selection = defaultUITheme()->selectionColor;
	int quads = 0;
	for (const UIVertex& v : h.ui.getDrawList().vertices()) {
		if (v.color == selection) quads++;
	}
	EXPECT_EQ(quads / 4, 3);
}

TEST(UITextArea, ScrollsToKeepTheCaretVisible) {
	UIHarness h;
	std::string text;
	for (int i = 0; i < 30; i++) text += "line\n";
	auto* area = h.root()->add<UITextArea>(text);
	area->setVisibleLines(5);
	h.click(area);
	h.press(UIKey::Home, UI_MOD_CONTROL);
	EXPECT_FLOAT_EQ(area->getScrollOffset(), 0.0f);
	h.press(UIKey::PageDown);
	h.press(UIKey::PageDown);
	EXPECT_GT(area->getScrollOffset(), 0.0f);
	h.press(UIKey::End, UI_MOD_CONTROL);
	EXPECT_FLOAT_EQ(area->getScrollOffset(), area->getMaxScroll());
	// 31 lines of 14px in a 5-line (70px) viewport.
	EXPECT_FLOAT_EQ(area->getMaxScroll(), 31 * 14 - 70);
}

TEST(UITextArea, PasteKeepsNewlines) {
	UIHarness h;
	auto* area = h.root()->add<UITextArea>();
	h.click(area);
	h.platform.clipboard = "a\r\nb\tc";
	h.press(UIKey::V, UI_MOD_CONTROL);
	EXPECT_EQ(area->getText(), "a\nb    c");
}

TEST(UITextArea, ClickPlacesCaretOnTheClickedLine) {
	UIHarness h;
	auto* area = h.root()->add<UITextArea>("first\nsecond");
	h.ui.layoutNow();
	const float pad = area->getPadding().left;
	// Second line, between "se" and "cond".
	h.click(h.atLocal(area, {pad + 14.0f, area->getPadding().top + 14.0f + 7.0f}));
	EXPECT_EQ(area->getEditState().caret(), 8u);
}
