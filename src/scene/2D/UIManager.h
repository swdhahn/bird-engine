#ifndef BIRD_UIMANAGER_H
#define BIRD_UIMANAGER_H

#include <memory>
#include <vector>

#include "../../animation/AnimationSystem.h"
#include "UIDrawList.h"
#include "UIEvents.h"
#include "UIObject.h"
#include "UIPlatform.h"

namespace bird {

class Font;

/** Where a popup opens relative to its anchor rect. */
enum class UIPopupPlacement : uint8_t {
	Below,	// under the anchor, flipping above when there is no room
	Right	// beside the anchor (submenus), flipping left
};

/**
 * Owns the UI tree and every piece of interaction state: hover, pointer
 * capture, keyboard focus, modal stack, tooltip and pending removals.
 *
 * Per frame: process() re-runs layout when invalidated (two O(n) passes),
 * advances timers, and rebuilds the draw list when invalidated. Input
 * arrives in window coordinates through mouseMove/mouseButton/scroll/key/
 * character; events go to the hit (or focused) node and bubble up through
 * its parents until a handler consumes them.
 *
 * Headless: no graphics API is touched, so the whole UI is unit-testable.
 */
class UIManager {
   public:
	/** Font baked by BuildTools and passed to init() by Application. */
	static constexpr const char* kDefaultFontPath =
		"assets/fonts/DejaVuSans.ttf";
	/** Seconds and pixels within which two presses form a double click. */
	static constexpr float kDoubleClickSeconds = 0.35f;
	static constexpr float kDoubleClickPixels = 4.0f;

	UIManager();
	~UIManager();

	/**
	 * @param font - UI font; its atlas also supplies the white texel.
	 * @param platform - clipboard and cursor services; may be nullptr.
	 * @throws std::invalid_argument when font is nullptr.
	 */
	void init(std::shared_ptr<Font> font, UIPlatform* platform);
	void deinit();

	/** Re-layouts, advances timers and rebuilds the draw list if needed. */
	void process(float delta);
	/** Runs any pending layout immediately (process() does this too). */
	void layoutNow();
	/** Resizes the root to the window so anchored widgets re-layout. */
	void setViewportSize(uint32_t width, uint32_t height);
	Vector2 getViewportSize() const { return m_viewport; }

	// ----------------------------------------------------------- input
	void mouseMove(Vector2 position, uint8_t modifiers = UI_MOD_NONE);
	void mouseButton(UIMouseButton button, bool pressed, Vector2 position,
					 uint8_t modifiers = UI_MOD_NONE);
	void scroll(Vector2 position, Vector2 delta);
	/** @param pressed - true for press and repeat, false for release. */
	void key(UIKey key, bool pressed, bool repeat,
			 uint8_t modifiers = UI_MOD_NONE, int keyCode = -1);
	void character(uint32_t codepoint);

	/** True while focused UI (e.g. a text box) should own the keyboard. */
	bool wantsKeyboard() const;
	/** True while the pointer is over interactive UI or captured by it. */
	bool wantsMouse() const;

	// ----------------------------------------------------------- focus
	void setFocus(UIObject* node);
	UIObject* getFocus() const { return m_focused; }
	/** Moves focus to the next (or previous) focusable node in tree order. */
	void focusNext(bool backwards = false);
	UIObject* getHovered() const { return m_hovered; }
	UIObject* getCaptured() const { return m_captured; }

	// --------------------------------------------------------- layers
	UIObject* getRoot();
	/** Layer drawn above the root: dialogs and tooltips. */
	UIObject* getOverlay();
	/**
	 * Shows a modal node on the overlay, stretched over the viewport. While
	 * any modal is open, input below the top modal is blocked.
	 * @return the modal, for further configuration.
	 */
	UIObject* pushModal(std::unique_ptr<UIObject> modal);
	/** Closes a modal opened with pushModal (deferred like removeChild). */
	void closeModal(UIObject* modal);
	UIObject* getTopModal() const;

	// --------------------------------------------------------- popups
	/**
	 * Opens a light-dismiss popup (dropdown list, menu) on the overlay,
	 * above any modal. It is positioned against anchorRect (screen space)
	 * after measuring, flipped and clamped to stay on screen. Pressing
	 * outside every popup closes them all; Escape closes the top one.
	 * Keys and characters reach the top popup before the focused node.
	 * @param owner - notified through onPopupClosed when it closes.
	 */
	UIObject* openPopup(UIObject* owner, std::unique_ptr<UIObject> popup,
						UIRect anchorRect,
						UIPopupPlacement placement = UIPopupPlacement::Below);
	/** Closes a popup and every popup opened after it (submenus). */
	void closePopup(UIObject* popup);
	void closeAllPopups();
	bool isPopupOpen(const UIObject* popup) const;
	UIObject* getTopPopup() const;
	size_t getPopupCount() const { return m_popups.size(); }
	/** Opens a context menu at a screen position. */
	UIObject* showContextMenu(UIMenuItems items, Vector2 position,
							  UIObject* owner = nullptr);

	/**
	 * Calls node->tick(dt) every frame until it returns false. Suited to
	 * timers and hand-written widget logic; prefer UIObject::animate()
	 * for value transitions.
	 */
	void animate(UIObject* node);

	/**
	 * Animation system for UI transitions, ticked by process() before
	 * layout. Register UI nodes as owners so removal cancels them.
	 */
	AnimationSystem& getAnimationSystem() { return m_animations; }

	/** Deepest node under a point that has a context menu, or null. */
	UIObject* findContextMenuTarget(Vector2 position) const;
	/** Topmost interactive node under a point, honouring the modal. */
	UIObject* hitTest(Vector2 position) const;
	/** Topmost interactive, enabled, visible node of a subtree. */
	static UIObject* findTopmostAt(UIObject* root, const Vector2& point);

	// ---------------------------------------------- node notifications
	void invalidateLayout();
	void invalidateDraw();
	/** A visual transform changed: recompose transforms, skip layout. */
	void invalidateTransforms();
	/** Legacy name for invalidateLayout(). */
	void setUpdateUIFlag() { invalidateLayout(); }
	/** Queues a node for destruction at the next safe point. */
	void scheduleRemoval(UIObject* node);
	/** Clears every interaction pointer into a subtree about to die. */
	void onSubtreeRemoved(UIObject* subtree);

	// ------------------------------------------------------- accessors
	const UIDrawList& getDrawList() const { return m_drawList; }
	const std::shared_ptr<Font>& getFont() const { return m_font; }
	UIPlatform* getPlatform() const { return m_platform; }
	float getTime() const { return m_time; }
	bool isLayoutDirty() const { return m_layoutDirty; }

   private:
	void flushRemovals();
	void rebuildDrawList();
	void drawTree(UIObject& node, float parentOpacity);
	void setHovered(UIObject* node);
	void updateCursor();
	void updateTooltip(float delta);
	void hideTooltip();
	/** Sends an event to node, then each ancestor, until one consumes it. */
	template <typename Handler>
	UIObject* bubble(UIObject* node, Handler handler);
	UIObject* focusScope();
	bool insidePopup(const UIObject* node) const;
	void placePopups();
	void tickAnimations(float delta);

	struct PopupEntry {
		UIObject* node;
		UIObject* owner;
		UIRect anchor;
		UIPopupPlacement placement;
	};

	std::unique_ptr<UIObject> m_root;
	std::unique_ptr<UIObject> m_overlay;
	std::shared_ptr<Font> m_font;
	UIPlatform* m_platform = nullptr;
	UIDrawList m_drawList;

	UIObject* m_hovered = nullptr;
	UIObject* m_captured = nullptr;
	UIObject* m_focused = nullptr;
	UIObject* m_tooltip = nullptr;
	UIObject* m_tooltipSource = nullptr;
	std::vector<UIObject*> m_modals;
	std::vector<PopupEntry> m_popups;
	std::vector<UIObject*> m_animating;
	std::vector<UIObject*> m_pendingRemovals;
	AnimationSystem m_animations;

	Vector2 m_viewport{0.0f};
	Vector2 m_mouse{0.0f};
	Vector2 m_lastClickPosition{-1e9f};
	float m_lastClickTime = -1e9f;
	int m_lastClickCount = 0;
	float m_time = 0.0f;
	float m_hoverTime = 0.0f;
	UICursor m_cursor = UICursor::Arrow;
	bool m_layoutDirty = true;
	bool m_drawDirty = true;
	bool m_transformsDirty = false;
	bool m_shuttingDown = false;
	bool m_swallowRelease = false;  // release after a light-dismiss press
};

}  // namespace bird

#endif	// !BIRD_UIMANAGER_H
