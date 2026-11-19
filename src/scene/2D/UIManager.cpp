#include "UIManager.h"

#include <algorithm>
#include <stdexcept>

#include "../../renders/generic/Font.h"
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
	if (UIObject* modal = getTopModal()) {
		return findTopmostAt(modal, position);
	}
	if (UIObject* hit = findTopmostAt(m_overlay.get(), position)) return hit;
	return findTopmostAt(m_root.get(), position);
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
		setHovered(hit);
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
	if (m_focused) m_focused->onChar(codepoint);
	flushRemovals();
}

bool UIManager::wantsKeyboard() const {
	return m_focused && m_focused->capturesKeyboard();
}

bool UIManager::wantsMouse() const {
	return m_captured || m_hovered || !m_modals.empty();
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
