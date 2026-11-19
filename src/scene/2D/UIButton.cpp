#include "UIButton.h"

#include "../Application.h"
#include "UIManager.h"
#include "UIObject.h"

namespace bird {

namespace {
constexpr int kBorderDrawable = 0;
constexpr int kFillDrawable = 1;
constexpr float kBorderInset = 0.05f;
}  // namespace

UIButton::UIButton() : UIObject(2) {
	m_drawables[kBorderDrawable] = {{0.0f, 0.0f}, {1.0f, 1.0f},
									m_theme->borderColor};
	m_drawables[kFillDrawable] = {{kBorderInset, kBorderInset},
								  {1.0f - 2 * kBorderInset,
								   1.0f - 2 * kBorderInset},
								  m_theme->surfaceColor};
}
UIButton::~UIButton() {}

void UIButton::onPress(float mx, float my) {
	m_pressed = true;
	refreshFillColor();
}

void UIButton::onRelease(float mx, float my) {
	m_pressed = false;
	refreshFillColor();
	if (m_onClick && containsPoint(Vector2(mx, my))) {
		m_onClick();
	}
}

void UIButton::onHover(bool hovered) {
	m_hovered = hovered;
	refreshFillColor();
}

void UIButton::setOnClick(std::function<void()> onClick) {
	m_onClick = std::move(onClick);
}

void UIButton::refreshFillColor() {
	const Vector3 color = m_pressed   ? m_theme->accentColor
						  : m_hovered ? m_theme->hoverColor
									  : m_theme->surfaceColor;
	if (m_drawables[kFillDrawable].color != color) {
		m_drawables[kFillDrawable].color = color;
		APPLICATION->getUIManager()->setUpdateUIFlag();
	}
}

}  // namespace bird
