#ifndef BIRD_UIOBJECT_H
#define BIRD_UIOBJECT_H

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ScreenObject.h"
#include "UIEvents.h"
#include "UILayout.h"
#include "UIPlatform.h"
#include "UITheme.h"

namespace bird {

class UIManager;
class UIDrawList;
class Font;

/** Fractions of the parent's content box, for anchors and pivots. */
struct UIAlignment {
	static constexpr Vector2 TopLeft{0.0f, 0.0f};
	static constexpr Vector2 TopCenter{0.5f, 0.0f};
	static constexpr Vector2 TopRight{1.0f, 0.0f};
	static constexpr Vector2 CenterLeft{0.0f, 0.5f};
	static constexpr Vector2 Center{0.5f, 0.5f};
	static constexpr Vector2 CenterRight{1.0f, 0.5f};
	static constexpr Vector2 BottomLeft{0.0f, 1.0f};
	static constexpr Vector2 BottomCenter{0.5f, 1.0f};
	static constexpr Vector2 BottomRight{1.0f, 1.0f};
};

/**
 * Base node of the retained UI tree.
 *
 * Layout runs in two O(n) passes driven by UIManager: measure() computes
 * each node's natural size bottom-up, arrange() places nodes top-down.
 * A plain UIObject is a free container: children are placed by anchors,
 * pivot and offset (setPosition) inside its padded content box. Equal
 * anchorMin/anchorMax place a child; different values stretch it.
 *
 * Coordinates: getSize() is the arranged border-box size in pixels; local
 * space has its origin at the node's top-left corner.
 */
class UIObject : public ScreenObject {
   public:
	UIObject();
	~UIObject() override;

	void init() override {}
	void deinit() override {}

	// ---------------------------------------------------------------- tree
	/** Appends a child and returns it for further configuration. */
	UIObject* addChild(std::unique_ptr<UIObject> child);
	/** Creates, appends and returns a child of type T. */
	template <typename T, typename... Args>
	T* add(Args&&... args) {
		return static_cast<T*>(
			addChild(std::make_unique<T>(std::forward<Args>(args)...)));
	}
	/**
	 * Removes and destroys a child. While attached to a UIManager the
	 * removal is deferred to a safe point, so handlers may close themselves.
	 */
	void removeChild(UIObject* child);
	void removeFromParent();
	/** Moves this node to the end of its siblings (drawn on top). */
	void bringToFront();

	std::vector<std::unique_ptr<UIObject>>& getChildren();
	UIObject* getParent() const;
	UIManager* getManager() const;
	bool isDescendantOf(const UIObject* ancestor) const;

	// ---------------------------------------------------- layout parameters
	UIObject& setWidth(UISizeRule rule);
	UIObject& setHeight(UISizeRule rule);
	/** Shorthand for fixed width and height in pixels. */
	UIObject& setSize(Vector2 pixels);
	UIObject& setMargin(UIInsets margin);
	UIObject& setPadding(UIInsets padding);
	/** Places this node at one anchor point (anchorMin = anchorMax). */
	UIObject& setAnchor(Vector2 anchor);
	/** Stretches this node between two anchor points of the parent. */
	UIObject& setAnchors(Vector2 anchorMin, Vector2 anchorMax);
	UIObject& setPivot(Vector2 pivot);
	/** Pixel offset from the anchor point (same as setPosition). */
	UIObject& setOffset(Vector2 offset);
	UIObject& setVisible(bool visible);
	UIObject& setEnabled(bool enabled);
	/** Text shown after hovering for UITheme::tooltipDelaySeconds. */
	UIObject& setTooltip(std::string tooltip);
	UIObject& setTheme(std::shared_ptr<UITheme> theme);

	const UISizeRule& getWidth() const { return m_width; }
	const UISizeRule& getHeight() const { return m_height; }
	const UIInsets& getMargin() const { return m_margin; }
	const UIInsets& getPadding() const { return m_padding; }
	const Vector2& getAnchorMin() const { return m_anchorMin; }
	const Vector2& getAnchorMax() const { return m_anchorMax; }
	const Vector2& getPivot() const { return m_pivot; }
	const std::string& getTooltip() const { return m_tooltip; }
	bool isVisible() const { return m_visible; }
	bool isEnabled() const { return m_enabled; }
	/** @return true when this node and every ancestor are enabled. */
	bool isEffectivelyEnabled() const;
	/** @return the size rule along one axis. */
	const UISizeRule& sizeRule(UIAxis axis) const;

	// ------------------------------------------------- layout (UIManager)
	/**
	 * Computes the natural border-box size (without margin).
	 * @param available - space offered, without this node's margin.
	 */
	Vector2 measure(Vector2 available);
	const Vector2& getDesiredSize() const { return m_desired; }
	/** Places this node; position is relative to the parent's top-left. */
	void arrange(Vector2 position, Vector2 size);
	/** Composes screen transforms and clip rects for this subtree. */
	void updateTransforms(const Matrix3& parentBasis, const UIRect& clip);

	// ------------------------------------------------------------ geometry
	Vector2 getSize() const { return m_scale; }
	/** Position relative to the parent's top-left, after layout. */
	const Vector2& getLayoutPosition() const { return m_layoutPosition; }
	/** Local (top-left origin, pixels) to screen transform. */
	const Matrix3& getWorldMatrix() const { return m_basis; }
	/** Clip rect (screen space) applied to this node by its ancestors. */
	const UIRect& getClipRect() const { return m_clip; }
	Vector2 toLocal(Vector2 screenPoint) const;
	/** @return true when the point is inside this node and its clip. */
	bool containsPoint(const Vector2& screenPoint) const;
	/** Local rect children are clipped to when clipping is enabled. */
	virtual UIRect childClipRect() const;
	bool clipsChildren() const { return m_clipChildren; }

	// ----------------------------------------------- behaviour (widgets)
	/** Receives pointer events (hit-testable). */
	bool isInteractive() const { return m_interactive; }
	virtual bool isFocusable() const { return false; }
	/** True while focused input should block game keyboard actions. */
	virtual bool capturesKeyboard() const { return false; }
	virtual UICursor cursor() const { return UICursor::Arrow; }

	// Handlers return true when they consumed the event; unconsumed
	// events bubble to the parent.
	virtual bool onMouseDown(const UIMouseEvent& event) { return false; }
	virtual bool onMouseUp(const UIMouseEvent& event) { return false; }
	virtual bool onMouseMove(const UIMouseEvent& event) { return false; }
	virtual bool onScroll(const UIScrollEvent& event) { return false; }
	virtual bool onKey(const UIKeyEvent& event) { return false; }
	virtual bool onChar(uint32_t codepoint) { return false; }
	virtual void onHover(bool hovered) {}
	virtual void onFocus(bool focused) {}
	/** Called every frame while this node has keyboard focus. */
	virtual void update(float delta) {}
	/** Emits this node's own primitives in local space. */
	virtual void draw(UIDrawList& list) {}
	/** Emits primitives drawn above the children (e.g. scrollbars). */
	virtual void drawOverlay(UIDrawList& list) {}

	// Hover/press/focus state, maintained by UIManager.
	bool isHovered() const { return m_hovered; }
	bool isPressed() const { return m_pressed; }
	bool isFocused() const { return m_focused; }

   protected:
	/** Natural content size (excluding padding). Default: free container. */
	virtual Vector2 measureContent(Vector2 available);
	/** Places children inside the content box. Default: anchors. */
	virtual void arrangeChildren();

	/** Measures every visible child with its margin removed. */
	void measureChildren(Vector2 available);
	/** Places a child by its anchors within the given content box. */
	void arrangeAnchored(UIObject& child, Vector2 contentOrigin,
						 Vector2 contentSize);

	void markLayoutDirty();
	void markVisualDirty();
	const UITheme& theme() const { return *m_theme; }
	/** @return the UI font, or nullptr when detached or headless. */
	const Font* font() const;
	UIPlatform* platform() const;
	Vector2 contentSize() const { return getSize() - m_padding.total(); }

	bool m_interactive = false;
	bool m_clipChildren = false;
	std::shared_ptr<UITheme> m_theme;

   private:
	friend class UIManager;
	void attach(UIManager* manager, UIObject* parent);
	void setManagerRecursive(UIManager* manager);
	/** Destroys a child immediately; UIManager calls this when safe. */
	void destroyChild(UIObject* child);

	std::vector<std::unique_ptr<UIObject>> m_children;
	UIObject* m_parent = nullptr;
	UIManager* m_manager = nullptr;

	UISizeRule m_width;
	UISizeRule m_height;
	UIInsets m_margin;
	UIInsets m_padding;
	Vector2 m_anchorMin{0.0f};
	Vector2 m_anchorMax{0.0f};
	Vector2 m_pivot{0.0f};
	std::string m_tooltip;
	bool m_visible = true;
	bool m_enabled = true;

	Vector2 m_desired{0.0f};
	Vector2 m_layoutPosition{0.0f};
	Matrix3 m_basis{1.0f};
	UIRect m_clip = UIRect::unbounded();

	bool m_hovered = false;
	bool m_pressed = false;
	bool m_focused = false;
};

}  // namespace bird

#endif	// !BIRD_UIOBJECT_H
