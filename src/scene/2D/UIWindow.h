#ifndef BIRD_UIWINDOW_H
#define BIRD_UIWINDOW_H

#include <functional>
#include <string>

#include "UIObject.h"

namespace bird {

class UIButton;

/**
 * A floating panel with a title bar. Drag the title bar to move it, drag
 * the bottom-right grip to resize it (not below the minimum size), press
 * the close button to remove it. Pressing anywhere brings it to the front.
 *
 * Add children to getContent(), a padded, clipped area below the title
 * bar. Place windows in a free container (e.g. the UI root); dragging moves
 * the window's offset from its anchor.
 */
class UIWindow : public UIObject {
   public:
	static constexpr float kGripSize = 14.0f;

	explicit UIWindow(std::string title, Vector2 size = Vector2(360, 280));

	UIObject* getContent() const { return m_content; }
	UIWindow& setTitle(std::string title);
	const std::string& getTitle() const { return m_title; }
	UIWindow& setResizable(bool resizable);
	UIWindow& setClosable(bool closable);
	UIWindow& setMinSize(Vector2 minSize);
	/** Called just before the window removes itself. */
	UIWindow& setOnClose(std::function<void()> onClose);
	void close();

	UICursor cursor() const override;
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	void draw(UIDrawList& list) override;
	void drawOverlay(UIDrawList& list) override;

   protected:
	void arrangeChildren() override;
	Vector2 measureContent(Vector2 available) override;

   private:
	enum class Drag { None, Move, Resize };
	bool inTitleBar(Vector2 local) const;
	bool inGrip(Vector2 local) const;

	std::string m_title;
	UIObject* m_content;
	UIButton* m_closeButton;
	Vector2 m_minSize{160.0f, 100.0f};
	bool m_resizable = true;
	Drag m_drag = Drag::None;
	Vector2 m_dragStartMouse{0.0f};
	Vector2 m_dragStartOffset{0.0f};
	Vector2 m_dragStartSize{0.0f};
	Vector2 m_lastLocalMouse{-1.0f};
	std::function<void()> m_onClose;
};

}  // namespace bird

#endif	// !BIRD_UIWINDOW_H
