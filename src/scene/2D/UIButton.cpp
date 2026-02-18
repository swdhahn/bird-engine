#include "UIButton.h"

#include "../Application.h"
#include "UIManager.h"
#include "UIObject.h"

namespace bird {

UIButton::UIButton() : UIObject(2) {
	m_drawables[0] = {{0.0f, 0.0f}, {1, 1}, {0.2f, 0.2f, 0.2f}};
	m_drawables[1] = {{0.05f, 0.05f}, {0.9, 0.9}, {0.8f, 0.2f, 0.2f}};
}
UIButton::~UIButton() {}

void UIButton::onPress(float mx, float my) {
	std::cout << Vector2(mx, my) << "   "
			  << Vector4(m_position.x, m_position.y, getScale().x, getScale().y)
			  << std::endl;
	if (contains(Vector2(mx, my), Vector4(m_position.x, m_position.y,
										  getScale().x, getScale().y))) {
		m_drawables[1].color = Vector3(0.2f, 0.8f, 0.2f);
		APPLICATION->getUIManager()->setUpdateUIFlag();
	}
}
void UIButton::onRelease(float mx, float my) {
	if (contains(Vector2(mx, my), Vector4(m_position.x, m_position.y,
										  getScale().x, getScale().y))) {
		m_drawables[1].color = Vector3(0.8f, 0.2f, 0.2f);
		APPLICATION->getUIManager()->setUpdateUIFlag();
	}
}
void UIButton::onMove(float mx, float my) {
	if (contains(Vector2(mx, my),
				 Vector4(m_position.x, m_position.y, m_bounds.x, m_bounds.y))) {
	}
}

}  // namespace bird
