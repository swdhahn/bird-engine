#ifndef BIRD_UIEXPANDER_H
#define BIRD_UIEXPANDER_H

#include <functional>
#include <string>

#include "UIStack.h"

namespace bird {

class UIExpanderHeader;

/**
 * Collapsible section: a clickable header with a disclosure triangle above
 * a content area. Collapsed content is hidden, so it takes no space and is
 * neither drawn nor hit-tested. Add children to getContent().
 */
class UIExpander : public UIStack {
   public:
	explicit UIExpander(std::string title, bool expanded = true);

	UIExpander& setExpanded(bool expanded);
	bool isExpanded() const { return m_expanded; }
	void toggle() { setExpanded(!m_expanded); }
	UIObject* getContent() const { return m_content; }
	UIExpanderHeader* getHeader() const { return m_header; }
	const std::string& getTitle() const { return m_title; }
	UIExpander& setOnToggled(std::function<void(bool)> onToggled);

   private:
	std::string m_title;
	bool m_expanded;
	UIExpanderHeader* m_header;
	UIObject* m_content;
	std::function<void(bool)> m_onToggled;
};

/** Header row of a UIExpander (focusable; click, Space or Enter toggle). */
class UIExpanderHeader : public UIObject {
   public:
	explicit UIExpanderHeader(UIExpander& owner);

	bool isFocusable() const override { return true; }
	UICursor cursor() const override { return UICursor::Hand; }
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	bool onKey(const UIKeyEvent& event) override;
	void draw(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;

   private:
	UIExpander& m_owner;
};

}  // namespace bird

#endif	// !BIRD_UIEXPANDER_H
