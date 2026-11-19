#include <gtest/gtest.h>

#include "UIHarness.h"
#include "scene/2D/UITextBox.h"
#include "scene/2D/UITextEditState.h"

using namespace bird;
using bird::test::UIHarness;

TEST(UITextEditState, InsertsAtCaret) {
	UITextEditState s;
	s.insert("hello");
	EXPECT_EQ(s.caret(), 5u);
	s.moveLeft(false);
	s.moveLeft(false);
	s.insert("X");
	EXPECT_EQ(s.text(), "helXlo");
	EXPECT_EQ(s.caret(), 4u);
}

TEST(UITextEditState, InsertReplacesSelection) {
	UITextEditState s;
	s.setText("hello world");
	s.selectWordAt(2);
	EXPECT_EQ(s.selectedText(), "hello");
	s.insert("bye");
	EXPECT_EQ(s.text(), "bye world");
}

TEST(UITextEditState, WordDeletionAndMovement) {
	UITextEditState s;
	s.setText("hello world");
	s.backspace(true);
	EXPECT_EQ(s.text(), "hello ");

	s.setText("hello world");
	s.setCaret(0, false);
	s.deleteForward(true);
	EXPECT_EQ(s.text(), " world");

	s.setText("hello world");
	s.setCaret(0, false);
	s.moveRight(false, true);
	EXPECT_EQ(s.caret(), 5u);
	s.moveRight(false, true);
	EXPECT_EQ(s.caret(), 11u);
	s.moveLeft(true, true);
	EXPECT_EQ(s.selectedText(), "world");
}

TEST(UITextEditState, ArrowCollapsesSelection) {
	UITextEditState s;
	s.setText("abcdef");
	s.selectAll();
	s.moveLeft(false);
	EXPECT_FALSE(s.hasSelection());
	EXPECT_EQ(s.caret(), 0u);
}

TEST(UITextEditState, MaxLengthTruncates) {
	UITextEditState s(5);
	s.insert("abcdefg");
	EXPECT_EQ(s.text(), "abcde");
	EXPECT_FALSE(s.insert("z"));
	s.setText("0123456789");
	EXPECT_EQ(s.text(), "01234");
}

TEST(UITextEditState, BackspaceAtStartDoesNothing) {
	UITextEditState s;
	s.setText("a");
	s.setCaret(0, false);
	EXPECT_FALSE(s.backspace());
	EXPECT_EQ(s.text(), "a");
}

TEST(UITextBox, TypingEditsAndNotifies) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>();
	int changes = 0;
	box->setOnChanged([&](const std::string&) { changes++; });
	h.click(box);
	h.type("abc");
	EXPECT_EQ(box->getText(), "abc");
	EXPECT_EQ(changes, 3);
	h.press(UIKey::Backspace);
	EXPECT_EQ(box->getText(), "ab");
}

TEST(UITextBox, IgnoresNonAsciiAndControlCharacters) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>();
	h.click(box);
	h.ui.character(0xE9);  // e acute
	h.ui.character('\n');
	EXPECT_EQ(box->getText(), "");
}

TEST(UITextBox, ClipboardCopyCutPaste) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>("abc");
	h.click(box);
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.press(UIKey::C, UI_MOD_CONTROL);
	EXPECT_EQ(h.platform.clipboard, "abc");
	h.press(UIKey::End);
	h.press(UIKey::V, UI_MOD_CONTROL);
	EXPECT_EQ(box->getText(), "abcabc");
	h.press(UIKey::A, UI_MOD_CONTROL);
	h.press(UIKey::X, UI_MOD_CONTROL);
	EXPECT_EQ(h.platform.clipboard, "abcabc");
	EXPECT_EQ(box->getText(), "");

	h.platform.clipboard = "one\ntwo";
	h.press(UIKey::V, UI_MOD_CONTROL);
	EXPECT_EQ(box->getText(), "one two");  // single line
}

TEST(UITextBox, EnterSubmits) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>("done");
	std::string submitted;
	box->setOnSubmit([&](const std::string& text) { submitted = text; });
	h.click(box);
	h.press(UIKey::Enter);
	EXPECT_EQ(submitted, "done");
}

TEST(UITextBox, ClickPlacesCaretAtNearestBoundary) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>("abcdef");
	h.ui.layoutNow();
	// Padding-left 8, 7px per glyph: boundary 2 sits at x = 22.
	EXPECT_EQ(box->caretIndexAt(8 + 14 + 1), 2u);
	EXPECT_EQ(box->caretIndexAt(8 + 14 + 4), 3u);
	EXPECT_EQ(box->caretIndexAt(-50), 0u);
	EXPECT_EQ(box->caretIndexAt(5000), 6u);
}

TEST(UITextBox, DoubleClickSelectsWord) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>("hello world");
	const Vector3 p =
		(h.ui.layoutNow(), box->getWorldMatrix()) *
		Vector3(8 + 7 * 8, box->getSize().y * 0.5f, 1.0f);
	const Vector2 position(p.x, p.y);
	h.click(position);
	h.click(position);
	EXPECT_EQ(box->getEditState().selectedText(), "world");
}

TEST(UITextBox, ShiftClickExtendsSelection) {
	UIHarness h;
	auto* box = h.root()->add<UITextBox>("abcdef");
	h.click(box);
	h.press(UIKey::Home);
	const Vector3 p = box->getWorldMatrix() *
					  Vector3(8 + 7 * 3, box->getSize().y * 0.5f, 1.0f);
	h.ui.mouseButton(UIMouseButton::Left, true, Vector2(p.x, p.y), UI_MOD_SHIFT);
	h.ui.mouseButton(UIMouseButton::Left, false, Vector2(p.x, p.y), UI_MOD_SHIFT);
	EXPECT_EQ(box->getEditState().selectedText(), "abc");
}
