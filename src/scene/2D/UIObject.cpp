#include "UIObject.h"

#include <algorithm>
#include <stdexcept>

#include "UIAnimator.h"
#include "UIGeometry.h"
#include "UIManager.h"

namespace bird {

namespace {

/** Places one axis: returns {start, length} within a content length. */
std::pair<float, float> placeAxis(float anchorMin, float anchorMax,
								  float pivot, float offset, float marginStart,
								  float marginEnd, const UISizeRule& rule,
								  float desired, float contentLength) {
	const float available =
		std::max(contentLength - marginStart - marginEnd, 0.0f);
	if (anchorMin != anchorMax) {
		const float length = rule.clamp((anchorMax - anchorMin) * available);
		return {marginStart + anchorMin * available + offset, length};
	}
	const float length =
		rule.mode == UISizeMode::Fill ? rule.clamp(available) : desired;
	return {marginStart + anchorMin * available + offset - pivot * length,
			length};
}

UIRect screenBounds(const Matrix3& basis, const UIRect& local) {
	const Vector2 corners[4] = {
		local.position, local.position + Vector2(local.size.x, 0.0f),
		local.position + Vector2(0.0f, local.size.y), local.max()};
	Vector2 low(1e30f), high(-1e30f);
	for (const Vector2& corner : corners) {
		const Vector3 p = basis * Vector3(corner, 1.0f);
		low = glm::min(low, Vector2(p));
		high = glm::max(high, Vector2(p));
	}
	return {low, high - low};
}

}  // namespace

UIObject::UIObject() : ScreenObject(), m_theme(defaultUITheme()) {
	m_scale = Vector2(0.0f);
}

UIObject::~UIObject() {
	// Let the manager drop any pointers into this subtree first.
	for (auto& child : m_children) {
		if (m_manager) m_manager->onSubtreeRemoved(child.get());
	}
	m_children.clear();
}

// ------------------------------------------------------------------ tree

UIObject* UIObject::addChild(std::unique_ptr<UIObject> child) {
	if (!child) {
		throw std::invalid_argument("UIObject::addChild: null child");
	}
	UIObject* raw = child.get();
	m_children.push_back(std::move(child));
	raw->attach(m_manager, this);
	markLayoutDirty();
	return raw;
}

void UIObject::attach(UIManager* manager, UIObject* parent) {
	m_parent = parent;
	setManagerRecursive(manager);
}

void UIObject::setManagerRecursive(UIManager* manager) {
	m_manager = manager;
	for (auto& child : m_children) child->setManagerRecursive(manager);
}

void UIObject::removeChild(UIObject* child) {
	if (!child || child->m_parent != this) {
		throw std::invalid_argument("UIObject::removeChild: not a child");
	}
	if (m_manager) {
		m_manager->scheduleRemoval(child);
	} else {
		destroyChild(child);
	}
}

void UIObject::destroyChild(UIObject* child) {
	auto it = std::find_if(
		m_children.begin(), m_children.end(),
		[child](const std::unique_ptr<UIObject>& c) { return c.get() == child; });
	if (it == m_children.end()) return;	 // already removed
	if (m_manager) m_manager->onSubtreeRemoved(child);
	m_children.erase(it);
	markLayoutDirty();
}

void UIObject::removeFromParent() {
	if (!m_parent) {
		throw std::logic_error("UIObject::removeFromParent: no parent");
	}
	m_parent->removeChild(this);
}

void UIObject::bringToFront() {
	if (!m_parent) return;
	auto& siblings = m_parent->m_children;
	auto it = std::find_if(
		siblings.begin(), siblings.end(),
		[this](const std::unique_ptr<UIObject>& c) { return c.get() == this; });
	if (it != siblings.end() && it + 1 != siblings.end()) {
		std::rotate(it, it + 1, siblings.end());
		markVisualDirty();
	}
}

std::vector<std::unique_ptr<UIObject>>& UIObject::getChildren() {
	return m_children;
}

UIObject* UIObject::getParent() const {
	return m_parent;
}

UIManager* UIObject::getManager() const {
	return m_manager;
}

bool UIObject::isDescendantOf(const UIObject* ancestor) const {
	for (const UIObject* node = this; node; node = node->m_parent) {
		if (node == ancestor) return true;
	}
	return false;
}

// ----------------------------------------------------- layout parameters

UIObject& UIObject::setWidth(UISizeRule rule) {
	m_width = rule;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setHeight(UISizeRule rule) {
	m_height = rule;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setSize(Vector2 pixels) {
	m_width = UISizeRule::fixed(pixels.x);
	m_height = UISizeRule::fixed(pixels.y);
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setMargin(UIInsets margin) {
	m_margin = margin;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setPadding(UIInsets padding) {
	m_padding = padding;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setAnchor(Vector2 anchor) {
	return setAnchors(anchor, anchor);
}

UIObject& UIObject::setAnchors(Vector2 anchorMin, Vector2 anchorMax) {
	m_anchorMin = anchorMin;
	m_anchorMax = anchorMax;
	m_anchorSet = true;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setPivot(Vector2 pivot) {
	m_pivot = pivot;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setOffset(Vector2 offset) {
	setPosition(offset);
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setLayoutMode(UILayoutMode mode) {
	m_layoutMode = mode;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setVisible(bool visible) {
	if (m_visible != visible) {
		m_visible = visible;
		markLayoutDirty();
	}
	return *this;
}

UIObject& UIObject::setEnabled(bool enabled) {
	if (m_enabled != enabled) {
		m_enabled = enabled;
		markVisualDirty();
	}
	return *this;
}

UIObject& UIObject::setTooltip(std::string tooltip) {
	m_tooltip = std::move(tooltip);
	return *this;
}

UIObject& UIObject::setClipChildren(bool clip) {
	m_clipChildren = clip;
	markLayoutDirty();
	return *this;
}

UIObject& UIObject::setContextMenu(std::vector<UIMenuItem> items) {
	m_contextMenu = items.empty() ? nullptr : makeMenu(std::move(items));
	return *this;
}

UIObject& UIObject::setTheme(std::shared_ptr<UITheme> theme) {
	if (!theme) throw std::invalid_argument("UIObject::setTheme: null theme");
	m_theme = std::move(theme);
	markLayoutDirty();
	return *this;
}

bool UIObject::isEffectivelyEnabled() const {
	for (const UIObject* node = this; node; node = node->m_parent) {
		if (!node->m_enabled) return false;
	}
	return true;
}

const UISizeRule& UIObject::sizeRule(UIAxis axis) const {
	return axis == UIAxis::Horizontal ? m_width : m_height;
}

// ---------------------------------------------------------------- layout

Vector2 UIObject::measure(Vector2 available) {
	const Vector2 inner =
		glm::max(available - m_padding.total(), Vector2(0.0f));
	const Vector2 content = measureContent(inner) + m_padding.total();
	m_desired.x = m_width.mode == UISizeMode::Fixed ? m_width.value : content.x;
	m_desired.y =
		m_height.mode == UISizeMode::Fixed ? m_height.value : content.y;
	m_desired =
		Vector2(m_width.clamp(m_desired.x), m_height.clamp(m_desired.y));
	return m_desired;
}

Vector2 UIObject::measureContent(Vector2 available) {
	measureChildren(available);
	Vector2 extent(0.0f);
	for (auto& child : m_children) {
		if (!child->m_visible) continue;
		extent = glm::max(extent, child->m_desired + child->m_margin.total());
	}
	return extent;
}

void UIObject::measureChildren(Vector2 available) {
	for (auto& child : m_children) {
		if (!child->m_visible) continue;
		child->measure(
			glm::max(available - child->m_margin.total(), Vector2(0.0f)));
	}
}

void UIObject::arrange(Vector2 position, Vector2 size) {
	m_layoutPosition = position;
	m_scale = glm::max(size, Vector2(0.0f));
	arrangeChildren();
}

void UIObject::arrangeChildren() {
	for (auto& child : m_children) {
		if (!child->m_visible) continue;
		arrangeAnchored(*child, m_padding.start(), contentSize());
	}
}

void UIObject::arrangeAnchored(UIObject& child, Vector2 contentOrigin,
							   Vector2 contentSize) {
	const auto [x, width] = placeAxis(
		child.m_anchorMin.x, child.m_anchorMax.x, child.m_pivot.x,
		child.m_position.x, child.m_margin.left, child.m_margin.right,
		child.m_width, child.m_desired.x, contentSize.x);
	const auto [y, height] = placeAxis(
		child.m_anchorMin.y, child.m_anchorMax.y, child.m_pivot.y,
		child.m_position.y, child.m_margin.top, child.m_margin.bottom,
		child.m_height, child.m_desired.y, contentSize.y);
	child.arrange(contentOrigin + Vector2(x, y), Vector2(width, height));
}

void UIObject::updateTransforms(const Matrix3& parentBasis,
								const UIRect& clip) {
	// Layout places the top-left corner; the visual transform then rotates
	// and scales around its origin and translates by its offset.
	const Vector2 origin = m_visual.origin * getSize();
	Matrix3 scale(1.0f);
	scale[0][0] = m_visual.scale.x;
	scale[1][1] = m_visual.scale.y;
	m_basis = parentBasis *
			  ui::makeBasis(m_layoutPosition + m_visual.offset + origin,
							m_rotation) *
			  scale * ui::makeBasis(-origin, 0.0f);
	m_clip = clip;
	const UIRect childClip =
		m_clipChildren ? clip.intersect(screenBounds(m_basis, childClipRect()))
					   : clip;
	for (auto& child : m_children) {
		if (child->m_visible) child->updateTransforms(m_basis, childClip);
	}
}

// -------------------------------------------------------------- geometry

Vector2 UIObject::toLocal(Vector2 screenPoint) const {
	const Vector3 local = glm::inverse(m_basis) * Vector3(screenPoint, 1.0f);
	return Vector2(local.x, local.y);
}

bool UIObject::containsPoint(const Vector2& screenPoint) const {
	if (!m_clip.contains(screenPoint)) return false;
	return UIRect{Vector2(0.0f), getSize()}.contains(toLocal(screenPoint));
}

UIRect UIObject::childClipRect() const {
	return {Vector2(0.0f), getSize()};
}

UIRect UIObject::getScreenRect() const {
	return screenBounds(m_basis, {Vector2(0.0f), getSize()});
}

// ----------------------------------------------------------------- misc

void UIObject::markLayoutDirty() {
	if (m_manager) m_manager->invalidateLayout();
}

void UIObject::markVisualDirty() {
	if (m_manager) m_manager->invalidateDraw();
}

void UIObject::markTransformDirty() {
	if (m_manager) m_manager->invalidateTransforms();
}

// ------------------------------------------------------- visual transform

UIObject& UIObject::setVisualOffset(Vector2 offset) {
	if (offset != m_visual.offset) {
		m_visual.offset = offset;
		markTransformDirty();
	}
	return *this;
}

UIObject& UIObject::setVisualScale(Vector2 scale) {
	if (scale != m_visual.scale) {
		m_visual.scale = scale;
		markTransformDirty();
	}
	return *this;
}

UIObject& UIObject::setVisualRotation(float radians) {
	if (radians != m_rotation) {
		m_rotation = radians;
		markTransformDirty();
	}
	return *this;
}

UIObject& UIObject::setVisualOrigin(Vector2 fraction) {
	if (fraction != m_visual.origin) {
		m_visual.origin = fraction;
		markTransformDirty();
	}
	return *this;
}

UIObject& UIObject::setOpacity(float opacity) {
	opacity = std::clamp(opacity, 0.0f, 1.0f);
	if (opacity != m_visual.opacity) {
		m_visual.opacity = opacity;
		markVisualDirty();
	}
	return *this;
}

UIAnimator UIObject::animate() {
	return UIAnimator(*this);
}

uint32_t UIObject::claimAnimated(UIAnimatedProperty property) {
	if (property >= UIAnimatedProperty::Count) {
		throw std::invalid_argument("UIObject::claimAnimated: bad property");
	}
	return ++m_animationStamps[static_cast<size_t>(property)];
}

bool UIObject::ownsAnimated(UIAnimatedProperty property,
							uint32_t stamp) const {
	return property < UIAnimatedProperty::Count &&
		   m_animationStamps[static_cast<size_t>(property)] == stamp;
}

const Font* UIObject::font() const {
	return m_manager ? m_manager->getFont().get() : nullptr;
}

UIPlatform* UIObject::platform() const {
	return m_manager ? m_manager->getPlatform() : nullptr;
}

}  // namespace bird
