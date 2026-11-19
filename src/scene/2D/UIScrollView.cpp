#include "UIScrollView.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

#include "UIDrawList.h"

namespace bird {

namespace {
constexpr float kMinThumbHeight = 20.0f;
}

UIScrollView::UIScrollView() {
	m_interactive = true;
	m_clipChildren = true;
}

UIObject* UIScrollView::setContent(std::unique_ptr<UIObject> content) {
	if (m_content) removeChild(m_content);
	m_content = addChild(std::move(content));
	m_offset = 0.0f;
	return m_content;
}

float UIScrollView::viewportHeight() const {
	return std::max(contentSize().y, 0.0f);
}

float UIScrollView::getMaxScroll() const {
	if (!m_content) return 0.0f;
	return std::max(m_content->getSize().y - viewportHeight(), 0.0f);
}

void UIScrollView::setScrollOffset(float offset) {
	const float clamped = std::clamp(offset, 0.0f, getMaxScroll());
	if (clamped != m_offset) {
		m_offset = clamped;
		markLayoutDirty();
	}
}

Vector2 UIScrollView::measureContent(Vector2 available) {
	if (!m_content || !m_content->isVisible()) return Vector2(0.0f);
	// Unbounded height: the content takes its natural height.
	const float width = std::max(available.x - theme().scrollbarWidth, 0.0f);
	m_content->measure(Vector2(width, std::numeric_limits<float>::infinity()));
	return m_content->getDesiredSize() + Vector2(theme().scrollbarWidth, 0.0f);
}

void UIScrollView::arrangeChildren() {
	if (!m_content || !m_content->isVisible()) return;
	const float width = std::max(contentSize().x - theme().scrollbarWidth, 0.0f);
	const float height =
		std::max(m_content->getDesiredSize().y, viewportHeight());
	m_offset = std::clamp(m_offset, 0.0f,
						  std::max(height - viewportHeight(), 0.0f));
	m_content->arrange(getPadding().start() - Vector2(0.0f, m_offset),
					   Vector2(width, height));
}

UIRect UIScrollView::childClipRect() const {
	return {getPadding().start(), contentSize()};
}

bool UIScrollView::onScroll(const UIScrollEvent& event) {
	const float before = m_offset;
	setScrollOffset(m_offset - event.delta.y * kWheelStep);
	return m_offset != before;	// at a limit, let an outer view scroll
}

UIRect UIScrollView::thumbRect() const {
	const float viewport = viewportHeight();
	const float total = viewport + getMaxScroll();
	const float height =
		std::max(viewport * viewport / std::max(total, 1.0f), kMinThumbHeight);
	const float travel = std::max(viewport - height, 0.0f);
	const float y = getMaxScroll() > 0.0f
						? travel * (m_offset / getMaxScroll())
						: 0.0f;
	const float width = theme().scrollbarWidth;
	return {Vector2(getSize().x - getPadding().right - width,
					getPadding().top + y),
			Vector2(width, height)};
}

bool UIScrollView::onMouseDown(const UIMouseEvent& event) {
	if (event.button != UIMouseButton::Left || getMaxScroll() <= 0.0f) {
		return false;
	}
	const Vector2 local = toLocal(event.position);
	const UIRect thumb = thumbRect();
	if (local.x < thumb.position.x) return false;  // not on the scrollbar
	if (thumb.contains(local)) {
		m_draggingThumb = true;
		m_dragStartMouseY = event.position.y;
		m_dragStartOffset = m_offset;
	} else {
		// Track click: page towards the pointer.
		const float page = viewportHeight() * 0.9f;
		setScrollOffset(m_offset +
						(local.y < thumb.position.y ? -page : page));
	}
	return true;
}

bool UIScrollView::onMouseMove(const UIMouseEvent& event) {
	if (!m_draggingThumb) return false;
	const float travel = viewportHeight() - thumbRect().size.y;
	if (travel > 0.0f) {
		const float delta = event.position.y - m_dragStartMouseY;
		setScrollOffset(m_dragStartOffset + delta / travel * getMaxScroll());
	}
	return true;
}

bool UIScrollView::onMouseUp(const UIMouseEvent& /*event*/) {
	m_draggingThumb = false;
	return true;
}

void UIScrollView::draw(UIDrawList& list) {
	list.roundedRect({Vector2(0.0f), getSize()}, theme().inputColor,
					 theme().controlRadius);
}

void UIScrollView::drawOverlay(UIDrawList& list) {
	if (getMaxScroll() <= 0.0f) return;
	const UIRect thumb = thumbRect();
	list.roundedRect(thumb, theme().scrollbarColor, thumb.size.x * 0.5f);
}

}  // namespace bird
