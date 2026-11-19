#include "UIObject.h"

#include "../Application.h"
#include "ScreenObject.h"
#include "UIGeometry.h"
#include "UIManager.h"

namespace bird {

UIObject::UIObject(size_t drawableCount)
	: ScreenObject(),
	  m_drawables(drawableCount),
	  m_theme(defaultUITheme()),
	  m_worldMatrix(1.0f),
	  m_childBasis(1.0f),
	  m_anchor(UIAlignment::TopLeft),
	  m_pivot(UIAlignment::TopLeft),
	  m_bounds(0, 0),
	  m_enabled(true),
	  m_interactive(true) {}
UIObject::~UIObject() {}

void UIObject::updateWorldMatrix(const Matrix3& parentBasis,
								 const Vector2& parentSize) {
	if (needsMatrixUpdate()) {
		updateTransformationMatrix();
	}
	m_childBasis = parentBasis * ui::makeAnchoredBasis(parentSize, m_anchor,
													   m_pivot, m_position,
													   m_rotation, m_scale);
	m_worldMatrix = m_childBasis * ui::makeScale(m_scale);
}

void UIObject::setAnchor(const Vector2& anchor) {
	m_anchor = anchor;
}
const Vector2& UIObject::getAnchor() const {
	return m_anchor;
}
void UIObject::setPivot(const Vector2& pivot) {
	m_pivot = pivot;
}
const Vector2& UIObject::getPivot() const {
	return m_pivot;
}

const Matrix3& UIObject::getChildBasis() const {
	return m_childBasis;
}

bool UIObject::interactive() const {
	return m_interactive;
}

const Matrix3& UIObject::getWorldMatrix() const {
	return m_worldMatrix;
}

bool UIObject::containsPoint(const Vector2& screenPoint) const {
	return ui::containsScreenPoint(m_worldMatrix, screenPoint);
}

bool UIObject::enabled() const {
	return m_enabled;
}

Vector2 UIObject::getBounds() const {
	return m_bounds;
}

UIDrawable* UIObject::getDrawables() {
	return m_drawables.data();
}

size_t UIObject::getDrawableCount() const {
	return m_drawables.size();
}

std::vector<std::unique_ptr<UIObject>>& UIObject::getChildren() {
	return m_children;
}

void UIObject::addChild(std::unique_ptr<UIObject> child) {
	m_children.emplace_back(std::move(child));
	APPLICATION->getUIManager()->setUpdateUIFlag();
}

}  // namespace bird
