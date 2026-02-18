#include "UIObject.h"

#include "../Application.h"
#include "ScreenObject.h"
#include "UIManager.h"

namespace bird {

UIObject::UIObject(uint8_t drawableCount)
	: ScreenObject(),
	  m_children(0),
	  m_enabled(true),
	  m_bounds(0, 0),
	  m_drawableCount(drawableCount),
	  m_drawables(std::make_unique<UIDrawable[]>(drawableCount)) {}
UIObject::~UIObject() {}

bool UIObject::enabled() const {
	return m_enabled;
}

Vector2 UIObject::getBounds() const {
	return m_bounds;
}

UIDrawable* UIObject::getDrawables() {
	return m_drawables.get();
}

uint8_t UIObject::getDrawableCount() {
	return m_drawableCount;
}

std::vector<std::unique_ptr<UIObject>>& UIObject::getChildren() {
	return m_children;
}

void UIObject::addChild(std::unique_ptr<UIObject> child) {
	m_children.emplace_back(std::move(child));
	APPLICATION->getUIManager()->setUpdateUIFlag();
}

}  // namespace bird
