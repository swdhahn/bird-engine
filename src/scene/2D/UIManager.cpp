#include "UIManager.h"

#include <algorithm>
#include <stdexcept>

#include "../../renders/generic/Font.h"
#include "UIMenuPanel.h"
#include "UITooltip.h"

namespace bird {

namespace {

/** Root and overlay layers: fixed to the viewport, never hit-testable. */
std::unique_ptr<UIObject> makeLayer() {
	auto layer = std::make_unique<UIObject>();
	layer->setSize(Vector2(0.0f));
	return layer;
}

void collectFocusable(UIObject* node, std::vector<UIObject*>& out) {
	if (!node->isVisible() || !node->isEnabled()) return;
	if (node->isFocusable()) out.push_back(node);
	for (auto& child : node->getChildren()) collectFocusable(child.get(), out);
}

}  // namespace

UIManager::UIManager()
	: m_root(makeLayer()), m_overlay(makeLayer()) {
	m_root->attach(this, nullptr);
	m_overlay->attach(this, nullptr);
}

UIManager::~UIManager() {
	m_shuttingDown = true;
	m_overlay.reset();
	m_root.reset();
}

void UIManager::init(std::shared_ptr<Font> font, UIPlatform* platform) {
	if (!font) {
		throw std::invalid_argument(
			"UIManager: font missing; run BuildTools to bake " +
			std::string(kDefaultFontPath));
	}
	m_font = std::move(font);
	m_platform = platform;
	m_drawList.setWhiteUV(m_font->getWhiteUV());
	invalidateLayout();
}

void UIManager::deinit() {}

// ---------------------------------------------------------------- frame

void UIManager::process(float delta) {
	m_time += delta;
	flushRemovals();
	tickAnimations(delta);
	flushRemovals();
	layoutNow();
	updateTooltip(delta);
	if (m_focused) m_focused->update(delta);
	layoutNow();  // tooltip or focus updates may have changed layout
	if (m_drawDirty) {
		rebuildDrawList();
		m_drawDirty = false;
	}
}

void UIManager::layoutNow() {
	if (!m_layoutDirty) return;
	m_layoutDirty = false;
	for (UIObject* layer : {m_root.get(), m_overlay.get()}) {
		layer->setSize(m_viewport);
		layer->measure(m_viewport);
		// Popups are placed against their anchors once their size is known.
		if (layer == m_overlay.get()) placePopups();
		layer->arrange(Vector2(0.0f), m_viewport);
		layer->updateTransforms(Matrix3(1.0f), UIRect::unbounded());
	}
	// Measuring may re-mark dirty through setters; the result is final.
	m_layoutDirty = false;
	m_drawDirty = true;
}

void UIManager::setViewportSize(uint32_t width, uint32_t height) {
	m_viewport = Vector2(static_cast<float>(width), static_cast<float>(height));
	invalidateLayout();
}

void UIManager::invalidateLayout() {
	m_layoutDirty = true;
	m_drawDirty = true;
}

void UIManager::invalidateDraw() {
	m_drawDirty = true;
}

void UIManager::rebuildDrawList() {
	m_drawList.clear();
	drawTree(*m_root);
	drawTree(*m_overlay);
}

void UIManager::drawTree(UIObject& node) {
	if (!node.isVisible()) return;
	m_drawList.setTransform(node.getWorldMatrix());
	node.draw(m_drawList);
	if (node.clipsChildren()) m_drawList.pushClip(node.childClipRect());
	for (auto& child : node.getChildren()) drawTree(*child);
	if (node.clipsChildren()) m_drawList.popClip();
	m_drawList.setTransform(node.getWorldMatrix());
	node.drawOverlay(m_drawList);
}

// -------------------------------------------------------------- removal

void UIManager::scheduleRemoval(UIObject* node) {
	if (std::find(m_pendingRemovals.begin(), m_pendingRemovals.end(), node) ==
		m_pendingRemovals.end()) {
		m_pendingRemovals.push_back(node);
	}
}

void UIManager::flushRemovals() {
	// Removing a node can schedule more (e.g. a dialog closing a child).
	while (!m_pendingRemovals.empty()) {
		UIObject* node = m_pendingRemovals.back();
		m_pendingRemovals.pop_back();
		if (UIObject* parent = node->getParent()) parent->destroyChild(node);
	}
}

void UIManager::onSubtreeRemoved(UIObject* subtree) {
	if (m_shuttingDown) return;
	auto inside = [subtree](UIObject* node) {
		return node && node->isDescendantOf(subtree);
	};
	if (inside(m_hovered)) m_hovered = nullptr;
	if (inside(m_captured)) m_captured = nullptr;
	if (inside(m_focused)) m_focused = nullptr;
	if (inside(m_tooltipSource)) m_tooltipSource = nullptr;
	if (inside(m_tooltip)) m_tooltip = nullptr;
	m_modals.erase(std::remove_if(m_modals.begin(), m_modals.end(), inside),
				   m_modals.end());
	m_animating.erase(
		std::remove_if(m_animating.begin(), m_animating.end(), inside),
		m_animating.end());
	// A popup dies with its node; a popup whose owner dies is closed.
	for (auto it = m_popups.begin(); it != m_popups.end();) {
		if (inside(it->node)) {
			it = m_popups.erase(it);
		} else if (inside(it->owner)) {
			scheduleRemoval(it->node);
			it = m_popups.erase(it);
		} else {
			++it;
		}
	}
	m_pendingRemovals.erase(
		std::remove_if(m_pendingRemovals.begin(), m_pendingRemovals.end(),
					   [subtree](UIObject* node) {
						   return node != subtree &&
								  node->isDescendantOf(subtree);
					   }),
		m_pendingRemovals.end());
	invalidateLayout();
}

// --------------------------------------------------------- hit testing

UIObject* UIManager::findTopmostAt(UIObject* root, const Vector2& point) {
	if (!root->isVisible()) return nullptr;
	auto& children = root->getChildren();
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if (UIObject* hit = findTopmostAt(it->get(), point)) return hit;
	}
	if (root->isInteractive() && root->isEffectivelyEnabled() &&
		root->containsPoint(point)) {
		return root;
	}
	return nullptr;
}

UIObject* UIManager::hitTest(Vector2 position) const {
	for (auto it = m_popups.rbegin(); it != m_popups.rend(); ++it) {
		if (UIObject* hit = findTopmostAt(it->node, position)) return hit;
	}
	if (UIObject* modal = getTopModal()) {
		return findTopmostAt(modal, position);
	}
	if (UIObject* hit = findTopmostAt(m_overlay.get(), position)) return hit;
	return findTopmostAt(m_root.get(), position);
}

namespace {
/** Deepest visible node under a point that has a context menu. */
UIObject* deepestWithMenu(UIObject* node, const Vector2& point) {
	if (!node->isVisible()) return nullptr;
	auto& children = node->getChildren();
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if (UIObject* found = deepestWithMenu(it->get(), point)) return found;
	}
	return node->getContextMenu() && node->containsPoint(point) ? node : nullptr;
}
}  // namespace

UIObject* UIManager::findContextMenuTarget(Vector2 position) const {
	// Context menus may live on non-interactive containers (pages, stacks),
	// so search by geometry rather than walking up from the hit node.
	if (UIObject* modal = getTopModal()) return deepestWithMenu(modal, position);
	return deepestWithMenu(m_root.get(), position);
}

template <typename Handler>
UIObject* UIManager::bubble(UIObject* node, Handler handler) {
	for (; node; node = node->getParent()) {
		if (node->isEffectivelyEnabled() && handler(node)) return node;
	}
	return nullptr;
}

// --------------------------------------------------------------- input

void UIManager::mouseMove(Vector2 position, uint8_t modifiers) {
	flushRemovals();
	layoutNow();
	m_mouse = position;
	const UIMouseEvent event{position, UIMouseButton::Left, modifiers, 1};
	if (m_captured) {
		m_captured->onMouseMove(event);
	} else {
		UIObject* hit = hitTest(position);
		setHovered(hit);
		bubble(hit, [&](UIObject* n) { return n->onMouseMove(event); });
	}
	updateCursor();
	flushRemovals();
}

void UIManager::mouseButton(UIMouseButton button, bool pressed,
							Vector2 position, uint8_t modifiers) {
	flushRemovals();
	layoutNow();
	m_mouse = position;
	UIMouseEvent event{position, button, modifiers, 1};
	if (pressed) {
		hideTooltip();
		const bool repeatClick =
			m_time - m_lastClickTime <= kDoubleClickSeconds &&
			glm::length(position - m_lastClickPosition) <= kDoubleClickPixels;
		event.clickCount = repeatClick ? m_lastClickCount + 1 : 1;
		m_lastClickCount = event.clickCount;
		m_lastClickTime = m_time;
		m_lastClickPosition = position;

		UIObject* hit = hitTest(position);
		// Light dismiss: a press outside every popup closes them and is
		// consumed (pressing a dropdown's own button therefore toggles it).
		if (!m_popups.empty() && !insidePopup(hit)) {
			closeAllPopups();
			m_swallowRelease = true;
			flushRemovals();
			updateCursor();
			return;
		}
		setHovered(hit);
		if (button == UIMouseButton::Right) {
			if (UIObject* target = findContextMenuTarget(position)) {
				showContextMenu(target->getContextMenu(), position, target);
				flushRemovals();
				return;
			}
		}
		// Focus the nearest focusable ancestor; clicking elsewhere blurs.
		UIObject* focusTarget = hit;
		while (focusTarget && !focusTarget->isFocusable()) {
			focusTarget = focusTarget->getParent();
		}
		setFocus(focusTarget);
		m_captured =
			bubble(hit, [&](UIObject* n) { return n->onMouseDown(event); });
		if (m_captured) {
			m_captured->m_pressed = true;
			invalidateDraw();
		}
	} else if (m_swallowRelease) {
		m_swallowRelease = false;  // the dismissing click ends here
	} else if (m_captured) {
		UIObject* target = m_captured;
		m_captured = nullptr;
		target->m_pressed = false;
		target->onMouseUp(event);
		invalidateDraw();
		setHovered(hitTest(position));
	} else {
		bubble(hitTest(position),
			   [&](UIObject* n) { return n->onMouseUp(event); });
	}
	updateCursor();
	flushRemovals();
}

void UIManager::scroll(Vector2 position, Vector2 delta) {
	flushRemovals();
	layoutNow();
	const UIScrollEvent event{position, delta};
	bubble(hitTest(position), [&](UIObject* n) { return n->onScroll(event); });
	flushRemovals();
}

void UIManager::key(UIKey key, bool pressed, bool repeat, uint8_t modifiers) {
	if (!pressed) return;
	flushRemovals();
	layoutNow();
	const UIKeyEvent event{key, modifiers, repeat};
	if (UIObject* popup = getTopPopup()) {
		if (key == UIKey::Tab) {
			closeAllPopups();  // then move focus as usual
		} else {
			// Popups own navigation keys while open.
			if (!popup->onKey(event) && key == UIKey::Escape) closePopup(popup);
			flushRemovals();
			return;
		}
	}
	const bool handled =
		bubble(m_focused, [&](UIObject* n) { return n->onKey(event); });
	if (!handled) {
		if (key == UIKey::Tab) {
			focusNext(event.shift());
		} else if (key == UIKey::Escape) {
			if (UIObject* modal = getTopModal()) {
				closeModal(modal);
			} else {
				setFocus(nullptr);
			}
		}
	}
	flushRemovals();
}

void UIManager::character(uint32_t codepoint) {
	flushRemovals();
	if (UIObject* popup = getTopPopup()) {
		popup->onChar(codepoint);  // type-ahead
	} else if (m_focused) {
		m_focused->onChar(codepoint);
	}
	flushRemovals();
}

bool UIManager::wantsKeyboard() const {
	return !m_popups.empty() || (m_focused && m_focused->capturesKeyboard());
}

bool UIManager::wantsMouse() const {
	return m_captured || m_hovered || !m_modals.empty() || !m_popups.empty();
}

// --------------------------------------------------------------- focus

void UIManager::setFocus(UIObject* node) {
	if (node == m_focused) return;
	if (m_focused) {
		m_focused->m_focused = false;
		m_focused->onFocus(false);
	}
	m_focused = node;
	if (m_focused) {
		m_focused->m_focused = true;
		m_focused->onFocus(true);
	}
	invalidateDraw();
}

UIObject* UIManager::focusScope() {
	if (UIObject* modal = getTopModal()) return modal;
	return m_root.get();
}

void UIManager::focusNext(bool backwards) {
	std::vector<UIObject*> order;
	collectFocusable(focusScope(), order);
	if (order.empty()) return;
	auto it = std::find(order.begin(), order.end(), m_focused);
	size_t index;
	if (it == order.end()) {
		index = backwards ? order.size() - 1 : 0;
	} else {
		const size_t current = static_cast<size_t>(it - order.begin());
		index = backwards ? (current + order.size() - 1) % order.size()
						  : (current + 1) % order.size();
	}
	setFocus(order[index]);
}

void UIManager::setHovered(UIObject* node) {
	if (node == m_hovered) return;
	if (m_hovered) {
		m_hovered->m_hovered = false;
		m_hovered->onHover(false);
	}
	m_hovered = node;
	m_hoverTime = 0.0f;
	if (m_tooltipSource != node) hideTooltip();
	if (m_hovered) {
		m_hovered->m_hovered = true;
		m_hovered->onHover(true);
	}
	invalidateDraw();
}

void UIManager::updateCursor() {
	UIObject* source = m_captured ? m_captured : m_hovered;
	const UICursor cursor = source ? source->cursor() : UICursor::Arrow;
	if (cursor != m_cursor) {
		m_cursor = cursor;
		if (m_platform) m_platform->setCursor(cursor);
	}
}

// -------------------------------------------------------------- layers

UIObject* UIManager::getRoot() {
	return m_root.get();
}

UIObject* UIManager::getOverlay() {
	return m_overlay.get();
}

UIObject* UIManager::pushModal(std::unique_ptr<UIObject> modal) {
	closeAllPopups();
	modal->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	UIObject* raw = m_overlay->addChild(std::move(modal));
	m_modals.push_back(raw);
	hideTooltip();
	setHovered(nullptr);
	setFocus(nullptr);
	focusNext();  // first focusable control inside the modal, if any
	return raw;
}

void UIManager::closeModal(UIObject* modal) {
	if (std::find(m_modals.begin(), m_modals.end(), modal) == m_modals.end()) {
		throw std::invalid_argument("UIManager::closeModal: not a modal");
	}
	m_modals.erase(std::remove(m_modals.begin(), m_modals.end(), modal),
				   m_modals.end());
	modal->removeFromParent();
}

UIObject* UIManager::getTopModal() const {
	return m_modals.empty() ? nullptr : m_modals.back();
}

// -------------------------------------------------------------- popups

UIObject* UIManager::openPopup(UIObject* owner, std::unique_ptr<UIObject> popup,
							   UIRect anchorRect, UIPopupPlacement placement) {
	if (!popup) throw std::invalid_argument("UIManager::openPopup: null");
	hideTooltip();
	UIObject* raw = m_overlay->addChild(std::move(popup));
	m_popups.push_back({raw, owner, anchorRect, placement});
	invalidateLayout();
	return raw;
}

void UIManager::closePopup(UIObject* popup) {
	auto it = std::find_if(m_popups.begin(), m_popups.end(),
						   [popup](const PopupEntry& e) { return e.node == popup; });
	if (it == m_popups.end()) return;
	// Close from the newest down to (and including) this popup.
	const size_t first = static_cast<size_t>(it - m_popups.begin());
	while (m_popups.size() > first) {
		const PopupEntry entry = m_popups.back();
		m_popups.pop_back();
		entry.node->removeFromParent();
		if (entry.owner) entry.owner->onPopupClosed(entry.node);
	}
}

void UIManager::closeAllPopups() {
	if (!m_popups.empty()) closePopup(m_popups.front().node);
}

bool UIManager::isPopupOpen(const UIObject* popup) const {
	return std::any_of(m_popups.begin(), m_popups.end(),
					   [popup](const PopupEntry& e) { return e.node == popup; });
}

UIObject* UIManager::getTopPopup() const {
	return m_popups.empty() ? nullptr : m_popups.back().node;
}

bool UIManager::insidePopup(const UIObject* node) const {
	if (!node) return false;
	return std::any_of(m_popups.begin(), m_popups.end(),
					   [node](const PopupEntry& e) {
						   return node->isDescendantOf(e.node);
					   });
}

UIObject* UIManager::showContextMenu(UIMenuItems items, Vector2 position,
									 UIObject* owner) {
	closeAllPopups();
	return openPopup(owner, std::make_unique<UIMenuPanel>(std::move(items)),
					 UIRect{position, Vector2(0.0f)}, UIPopupPlacement::Below);
}

void UIManager::placePopups() {
	for (const PopupEntry& entry : m_popups) {
		const Vector2 size = entry.node->getDesiredSize();
		const UIRect& a = entry.anchor;
		Vector2 p;
		if (entry.placement == UIPopupPlacement::Below) {
			p = Vector2(a.position.x, a.max().y);
			if (p.y + size.y > m_viewport.y && a.position.y - size.y >= 0.0f) {
				p.y = a.position.y - size.y;  // flip above
			}
		} else {
			p = Vector2(a.max().x, a.position.y);
			if (p.x + size.x > m_viewport.x && a.position.x - size.x >= 0.0f) {
				p.x = a.position.x - size.x;  // flip left
			}
		}
		p = glm::clamp(p, Vector2(0.0f), glm::max(m_viewport - size, Vector2(0.0f)));
		// Set directly: placement must not re-dirty the layout it is part of.
		entry.node->setPosition(p);
	}
}

// ----------------------------------------------------------- animation

void UIManager::animate(UIObject* node) {
	if (std::find(m_animating.begin(), m_animating.end(), node) ==
		m_animating.end()) {
		m_animating.push_back(node);
	}
}

void UIManager::tickAnimations(float delta) {
	// Iterate a copy: tick() may start or stop other animations.
	const std::vector<UIObject*> active = m_animating;
	for (UIObject* node : active) {
		if (std::find(m_animating.begin(), m_animating.end(), node) ==
			m_animating.end()) {
			continue;
		}
		if (!node->tick(delta)) {
			m_animating.erase(
				std::remove(m_animating.begin(), m_animating.end(), node),
				m_animating.end());
		}
		invalidateDraw();
	}
}

// ------------------------------------------------------------- tooltip

void UIManager::updateTooltip(float delta) {
	if (!m_hovered || m_captured || m_hovered->getTooltip().empty()) return;
	m_hoverTime += delta;
	if (m_tooltip || m_hoverTime < m_hovered->theme().tooltipDelaySeconds) {
		return;
	}
	auto tooltip = std::make_unique<UITooltip>(m_hovered->getTooltip());
	tooltip->setOffset(m_mouse + Vector2(12.0f, 18.0f));
	m_tooltipSource = m_hovered;
	m_tooltip = m_overlay->addChild(std::move(tooltip));
	layoutNow();
	// Keep the tooltip on screen: flip left/up when it would overflow.
	Vector2 offset = m_mouse + Vector2(12.0f, 18.0f);
	const Vector2 size = m_tooltip->getSize();
	if (offset.x + size.x > m_viewport.x) offset.x = m_mouse.x - size.x - 4.0f;
	if (offset.y + size.y > m_viewport.y) offset.y = m_mouse.y - size.y - 4.0f;
	m_tooltip->setOffset(glm::max(offset, Vector2(0.0f)));
}

void UIManager::hideTooltip() {
	if (m_tooltip) {
		UIObject* tooltip = m_tooltip;
		m_tooltip = nullptr;
		m_tooltipSource = nullptr;
		tooltip->removeFromParent();
	}
}

}  // namespace bird
