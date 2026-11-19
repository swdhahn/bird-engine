#include "UIManager.h"

#include <memory>
#include <stdexcept>

#include "../../Application.h"
#include "../../renders/generic/Font.h"
#include "../../renders/generic/Mesh.h"
#include "UIGeometry.h"
#include "UIObject.h"

namespace bird {

UIManager::UIManager()
	: m_root(std::make_unique<UIObject>(0)),
	  m_hovered(nullptr),
	  m_pressed(nullptr),
	  m_vertexCount(0),
	  m_indexCount(0),
	  m_shouldUpdateUI(true) {}
UIManager::~UIManager() {}

void UIManager::init(std::shared_ptr<Font> font) {
	if (!font) {
		throw std::invalid_argument(
			"UIManager: font missing; run BuildTools to bake " +
			std::string(kDefaultFontPath));
	}
	m_font = std::move(font);

	// Every buffer starts non-empty so that GLMesh binds all attribute
	// locations (0 position, 1 colour, 2 texCoord); update() grows them.
	const auto mode =
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC | BUFFER_ARRAY);
	m_mesh = MeshBuilder()
				 .setIndexBuffer(createBuffer<uint32_t>(
					 static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
											 BUFFER_ELEMENT_ARRAY),
					 1))
				 .setVertexBuffer(createBuffer<float>(mode, 1))
				 .setTextureCoordBuffer(createBuffer<float, 2>(mode, 1))
				 // The normal slot carries per-vertex RGB colour.
				 .setNormalBuffer(createBuffer<float>(mode, 1))
				 .build();
}
void UIManager::deinit() {}

void UIManager::process(float delta) {
	if (m_shouldUpdateUI) {
		updateUI();
		m_shouldUpdateUI = false;
	}
}

UIObject* UIManager::findTopmostAt(UIObject* root, const Vector2& screenPoint) {
	auto& children = root->getChildren();
	for (auto it = children.rbegin(); it != children.rend(); ++it) {
		if (UIObject* hit = findTopmostAt(it->get(), screenPoint)) {
			return hit;
		}
	}
	if (root->enabled() && root->interactive() &&
		root->getDrawableCount() > 0 &&
		root->containsPoint(screenPoint)) {
		return root;
	}
	return nullptr;
}

void UIManager::processMouseUI(double mx, double my, int action) {
	const Vector2 point(static_cast<float>(mx), static_cast<float>(my));
	UIObject* hit = findTopmostAt(m_root.get(), point);

	switch (action) {
		case ACTION_NONE:
			if (hit != m_hovered) {
				if (m_hovered) m_hovered->onHover(false);
				if (hit) hit->onHover(true);
				m_hovered = hit;
			}
			if (UIObject* target = m_pressed ? m_pressed : m_hovered) {
				target->onMove(point.x, point.y);
			}
			break;
		case ACTION_PRESS:
			if (hit) {
				m_pressed = hit;
				hit->onPress(point.x, point.y);
			}
			break;
		case ACTION_RELEASE:
			if (m_pressed) {
				m_pressed->onRelease(point.x, point.y);
				m_pressed = nullptr;
			}
			break;
		default:
			break;
	}
}

void UIManager::updateUI() {
	m_vertexCount = 0;
	m_vertices.clear();
	m_textureCoords.clear();
	m_colors.clear();
	m_indices.clear();

	processTreeUI(m_root.get(), Matrix3(1.0f), m_root->getScale());
	m_indexCount = static_cast<uint32_t>(m_indices.size());
	if (m_indexCount == 0) return;

	m_mesh->getVertexBuffer()->update(m_vertices.data(), m_vertices.size(), 0);
	m_mesh->getTextureCoordBuffer()->update(m_textureCoords.data(),
											m_textureCoords.size(), 0);
	m_mesh->getNormalBuffer()->update(m_colors.data(), m_colors.size(), 0);
	m_mesh->getIndexBuffer()->update(m_indices.data(), m_indices.size(), 0);
	m_mesh->updateMesh();
}

void UIManager::processTreeUI(UIObject* object, const Matrix3& parentBasis,
							  const Vector2& parentSize) {
	object->layout();
	object->updateWorldMatrix(parentBasis, parentSize);
	const Matrix3& world = object->getWorldMatrix();
	const Vector2 whiteUV = m_font->getWhiteUV();

	for (int j = 0; j < object->getDrawableCount(); j++) {
		const UIDrawable& draw = object->getDrawables()[j];
		if (!draw.visible) continue;

		const auto corners =
			ui::computeDrawableCorners(world, draw.localOffset, draw.size);
		for (size_t i = 0; i < corners.size(); i++) {
			m_vertices.insert(m_vertices.end(), {corners[i].x, corners[i].y, 0});
			const Vector2 uv =
				draw.textured ? glm::mix(draw.uvMin, draw.uvMax,
										 ui::kUnitQuadCorners[i])
							  : whiteUV;
			m_textureCoords.insert(m_textureCoords.end(), {uv.x, uv.y});
			m_colors.insert(m_colors.end(),
							{draw.color.x, draw.color.y, draw.color.z});
		}
		for (uint32_t index : ui::kQuadIndexPattern) {
			m_indices.push_back(m_vertexCount + index);
		}
		m_vertexCount += 4;
	}

	for (auto& child : object->getChildren()) {
		processTreeUI(child.get(), object->getChildBasis(), object->getScale());
	}
}

std::shared_ptr<Mesh> UIManager::getUIMesh() {
	return m_mesh;
}

const std::shared_ptr<Font>& UIManager::getFont() const {
	return m_font;
}

uint32_t UIManager::getIndexCount() const {
	return m_indexCount;
}

UIObject* UIManager::getRoot() {
	return m_root.get();
}

void UIManager::setUpdateUIFlag() {
	m_shouldUpdateUI = true;
}

void UIManager::setViewportSize(uint32_t width, uint32_t height) {
	m_root->setScale(
		Vector2(static_cast<float>(width), static_cast<float>(height)));
	m_shouldUpdateUI = true;
}

}  // namespace bird
