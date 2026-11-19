#ifndef BIRD_TESTS_UIHARNESS_H
#define BIRD_TESTS_UIHARNESS_H

#include <string>

#include "FakeFont.h"
#include "scene/2D/UIManager.h"

namespace bird::test {

/** Records clipboard and cursor requests instead of touching the OS. */
struct FakePlatform : UIPlatform {
	std::string clipboard;
	UICursor cursor = UICursor::Arrow;

	std::string getClipboardText() override { return clipboard; }
	void setClipboardText(const std::string& text) override {
		clipboard = text;
	}
	void setCursor(UICursor c) override { cursor = c; }
};

/**
 * Headless UI: a UIManager with the fake font (glyphs advance 7px and lines
 * are 14px tall at the default 14px size) and a fake platform. No GL.
 */
struct UIHarness {
	FakePlatform platform;
	UIManager ui;

	explicit UIHarness(Vector2 viewport = Vector2(800.0f, 600.0f)) {
		ui.init(makeFakeFont(), &platform);
		ui.setViewportSize(static_cast<uint32_t>(viewport.x),
						   static_cast<uint32_t>(viewport.y));
		ui.layoutNow();
	}

	UIObject* root() { return ui.getRoot(); }
	void frame(float delta = 0.016f) { ui.process(delta); }

	/** Screen position of a node's centre (or another local point). */
	Vector2 at(UIObject* node, Vector2 fraction = Vector2(0.5f)) {
		ui.layoutNow();
		const Vector3 p =
			node->getWorldMatrix() * Vector3(node->getSize() * fraction, 1.0f);
		return Vector2(p.x, p.y);
	}
	void click(Vector2 position, uint8_t modifiers = UI_MOD_NONE) {
		ui.mouseMove(position);
		ui.mouseButton(UIMouseButton::Left, true, position, modifiers);
		ui.mouseButton(UIMouseButton::Left, false, position, modifiers);
	}
	void click(UIObject* node) { click(at(node)); }
	void press(UIKey key, uint8_t modifiers = UI_MOD_NONE) {
		ui.key(key, true, false, modifiers);
	}
	void type(const std::string& text) {
		for (char c : text) ui.character(static_cast<unsigned char>(c));
	}
};

}  // namespace bird::test

#endif	// !BIRD_TESTS_UIHARNESS_H
