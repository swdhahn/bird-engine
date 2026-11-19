#include "UIWindow.h"

#include <algorithm>

#include "UIButton.h"
#include "UIDrawList.h"
#include "UIPanel.h"
#include "UITextUtil.h"

namespace bird {

namespace {
constexpr float kCloseButtonSize = 22.0f;
}

UIWindow::UIWindow(std::string title, Vector2 size) : m_title(std::move(title)) {
	m_interactive = true;
	setSize(size);

	auto* content = add<UIPanel>();
	content->setBackground(Vector4(0.0f));
	content->setBorder(false);
	content->setClipChildren(true);
	content->setAnchors(UIAlignment::TopLeft, UIAlignment::BottomRight);
	m_content = content;

	m_closeButton = add<UIButton>("x");
	m_closeButton->setPadding(UIInsets{});
	m_closeButton->setSize(Vector2(kCloseButtonSize));
	m_closeButton->setOnClick([this] { close(); });
}

UIWindow& UIWindow::setTitle(std::string title) {
	m_title = std::move(title);
	markVisualDirty();
	return *this;
}

UIWindow& UIWindow::setResizable(bool resizable) {
	m_resizable = resizable;
	markVisualDirty();
	return *this;
}

UIWindow& UIWindow::setClosable(bool closable) {
	m_closeButton->setVisible(closable);
	return *this;
}

UIWindow& UIWindow::setMinSize(Vector2 minSize) {
	m_minSize = minSize;
	setSize(glm::max(getSize(), minSize));
	return *this;
}

UIWindow& UIWindow::setOnClose(std::function<void()> onClose) {
	m_onClose = std::move(onClose);
	return *this;
}

void UIWindow::close() {
	if (m_onClose) m_onClose();
	removeFromParent();
}

Vector2 UIWindow::measureContent(Vector2 available) {
	measureChildren(available);
	return m_content->getDesiredSize() +
		   Vector2(0.0f, theme().titleBarHeight);
}

void UIWindow::arrangeChildren() {
	const float titleHeight = theme().titleBarHeight;
	arrangeAnchored(*m_content, Vector2(0.0f, titleHeight),
					getSize() - Vector2(0.0f, titleHeight));
	if (m_closeButton->isVisible()) {
		const float inset = (titleHeight - kCloseButtonSize) * 0.5f;
		m_closeButton->arrange(
			Vector2(getSize().x - kCloseButtonSize - inset, inset),
			Vector2(kCloseButtonSize));
	}
}

bool UIWindow::inTitleBar(Vector2 local) const {
	return local.y >= 0.0f && local.y < theme().titleBarHeight;
}

bool UIWindow::inGrip(Vector2 local) const {
	return m_resizable && local.x >= getSize().x - kGripSize &&
		   local.y >= getSize().y - kGripSize;
}

UICursor UIWindow::cursor() const {
	if (m_drag == Drag::Resize || inGrip(m_lastLocalMouse)) {
		return UICursor::ResizeDiagonal;
	}
	return UICursor::Arrow;
}

bool UIWindow::onMouseDown(const UIMouseEvent& event) {
	bringToFront();
	if (event.button != UIMouseButton::Left) return true;
	const Vector2 local = toLocal(event.position);
	m_dragStartMouse = event.position;
	m_dragStartOffset = getPosition();
	m_dragStartSize = getSize();
	if (inGrip(local)) {
		m_drag = Drag::Resize;
	} else if (inTitleBar(local)) {
		m_drag = Drag::Move;
	}
	return true;  // consume clicks on the window body
}

bool UIWindow::onMouseMove(const UIMouseEvent& event) {
	m_lastLocalMouse = toLocal(event.position);
	const Vector2 delta = event.position - m_dragStartMouse;
	if (m_drag == Drag::Move) {
		setOffset(m_dragStartOffset + delta);
	} else if (m_drag == Drag::Resize) {
		setSize(glm::max(m_dragStartSize + delta, m_minSize));
	} else {
		return false;
	}
	return true;
}

bool UIWindow::onMouseUp(const UIMouseEvent& /*event*/) {
	m_drag = Drag::None;
	return true;
}

void UIWindow::draw(UIDrawList& list) {
	const UITheme& t = theme();
	const UIRect bounds{Vector2(0.0f), getSize()};
	const UIRect title{Vector2(0.0f), Vector2(getSize().x, t.titleBarHeight)};
	list.shadow(bounds, t.panelRadius, t.shadowBlur, t.shadowColor);
	list.roundedRect(bounds, t.panelColor, t.panelRadius);
	// Title bar: rounded top corners, square bottom edge.
	list.roundedRect(title, t.titleBarColor, t.panelRadius);
	list.rect({Vector2(0.0f, title.size.y * 0.5f),
			   Vector2(title.size.x, title.size.y * 0.5f)},
			  t.titleBarColor);
	ui::drawTextCenteredY(list, font(), m_title, t.titleFontSize, t.padding,
						  title, t.textColor);
	list.roundedRect(bounds, Vector4(0.0f), t.panelRadius, t.borderWidth,
					 t.borderColor);
}

void UIWindow::drawOverlay(UIDrawList& list) {
	if (!m_resizable) return;
	// Three short diagonal steps in the bottom-right corner.
	const Vector2 corner = getSize() - Vector2(3.0f);
	for (int i = 1; i <= 3; i++) {
		const float s = static_cast<float>(i) * 3.5f;
		list.rect({corner - Vector2(s, 1.5f), Vector2(s, 1.5f)},
				  theme().scrollbarColor);
	}
}

}  // namespace bird
