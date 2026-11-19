#ifndef BIRD_UISCROLLVIEW_H
#define BIRD_UISCROLLVIEW_H

#include <memory>

#include "UIObject.h"

namespace bird {

/**
 * Vertically scrolling viewport around a single content child. The content
 * keeps its natural height; anything outside the viewport is clipped and
 * not hit-testable. Mouse wheel scrolls (unconsumed at the limits, so outer
 * scroll views take over); the scrollbar thumb can be dragged, and clicking
 * the track pages.
 */
class UIScrollView : public UIObject {
   public:
	static constexpr float kWheelStep = 40.0f;

	UIScrollView();

	/** Sets (replacing) the scrolled content and returns it. */
	UIObject* setContent(std::unique_ptr<UIObject> content);
	template <typename T, typename... Args>
	T* setContent(Args&&... args) {
		return static_cast<T*>(
			setContent(std::make_unique<T>(std::forward<Args>(args)...)));
	}
	UIObject* getContent() const { return m_content; }

	float getScrollOffset() const { return m_offset; }
	/** Clamped to 0..getMaxScroll(). */
	void setScrollOffset(float offset);
	float getMaxScroll() const;

	bool onScroll(const UIScrollEvent& event) override;
	bool onMouseDown(const UIMouseEvent& event) override;
	bool onMouseMove(const UIMouseEvent& event) override;
	bool onMouseUp(const UIMouseEvent& event) override;
	UIRect childClipRect() const override;
	void draw(UIDrawList& list) override;
	void drawOverlay(UIDrawList& list) override;

   protected:
	Vector2 measureContent(Vector2 available) override;
	void arrangeChildren() override;

   private:
	float viewportHeight() const;
	UIRect thumbRect() const;

	UIObject* m_content = nullptr;
	float m_offset = 0.0f;
	bool m_draggingThumb = false;
	float m_dragStartMouseY = 0.0f;
	float m_dragStartOffset = 0.0f;
};

}  // namespace bird

#endif	// !BIRD_UISCROLLVIEW_H
