#ifndef BIRD_UIDIALOG_H
#define BIRD_UIDIALOG_H

#include <functional>
#include <string>

#include "UIObject.h"

namespace bird {

class UIStack;
class UIPanel;

/**
 * Modal message box: a dimmed backdrop that swallows input plus a centred
 * panel with a title, a message and a row of buttons. Show it with
 * UIManager::pushModal; any button closes it after running its callback,
 * as does Escape.
 */
class UIDialog : public UIObject {
   public:
	UIDialog(std::string title, std::string message);

	/** Adds a button; the dialog closes after onClick runs. */
	UIDialog& addButton(std::string text, std::function<void()> onClick = {});
	/** Body area above the buttons, for extra controls. */
	UIStack* getBody() const { return m_body; }
	void close();

	bool onMouseDown(const UIMouseEvent& event) override { return true; }
	bool onMouseUp(const UIMouseEvent& event) override { return true; }
	bool onScroll(const UIScrollEvent& event) override { return true; }
	void draw(UIDrawList& list) override;

   private:
	UIPanel* m_panel;
	UIStack* m_body;
	UIStack* m_buttons;
};

}  // namespace bird

#endif	// !BIRD_UIDIALOG_H
