#include "UIManager.h"

#include <memory>

#include "../../Application.h"
#include "../../renders/GraphicsPipeline.h"
#include "../../renders/Window.h"
#include "../../renders/generic/Mesh.h"
#include "UIObject.h"

namespace bird {

UIManager::UIManager()
	: m_vertexCount(0), m_root(std::make_unique<UIObject>(0)) {}
UIManager::~UIManager() {}

void UIManager::init() {
	// We need to trick the mesh into thinking we have data
	m_mesh = MeshBuilder()
				 .setIndexBuffer(createBuffer<uint32_t>(
					 static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
											 BUFFER_ELEMENT_ARRAY),
					 1))
				 .setVertexBuffer(createBuffer<float>(
					 static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
											 BUFFER_ARRAY),
					 1))
				 .setTextureCoordBuffer(createBuffer<float, 2>(
					 static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
											 BUFFER_ARRAY),
					 1))
				 // Colors as normals
				 .setNormalBuffer(createBuffer<float>(
					 static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
											 BUFFER_ARRAY),
					 1))
				 .build();
}
void UIManager::deinit() {}

void UIManager::process(float delta) {
	if (m_shouldUpdateUI) {
		updateUI();
		m_shouldUpdateUI = false;
	}
}

void UIManager::processMouseUI(double mx, double my, int action) {
	processMouseUI(m_root.get(), mx, my, action);
}
void UIManager::processMouseUI(UIObject* root, double mx, double my,
							   int action) {
	if (action == ACTION_NONE) root->onMove(mx, my);
	if (action == ACTION_PRESS) root->onPress(mx, my);
	if (action == ACTION_RELEASE) root->onRelease(mx, my);

	for (int i = 0; i < root->getChildren().size(); i++) {
		processMouseUI(root->getChildren()[i].get(), mx, my, action);
	}
}

void UIManager::updateUI() {
	m_vertexCount = 0;
	m_vertices.clear();
	m_textureCoords.clear();
	m_colors.clear();
	m_indices.clear();
	for (int i = 0; i < m_root->getChildren().size(); i++) {
		processTreeUI(m_root->getChildren()[i].get());
	}
	// send to gpu

	m_mesh->getVertexBuffer()->update(m_vertices.data(), m_vertices.size(), 0);
	m_mesh->getTextureCoordBuffer()->update(m_textureCoords.data(),
											m_textureCoords.size(), 0);
	m_mesh->getNormalBuffer()->update(m_colors.data(), m_colors.size(), 0);
	m_mesh->getIndexBuffer()->update(m_indices.data(), m_indices.size(), 0);
	m_mesh->updateMesh();
}

void UIManager::processTreeUI(UIObject* root) {
	Vector3* baseVertices =
		((Vector3*)GraphicsPipeline::s_quad_mesh->getVertexBuffer()
			 ->m_rawBuffer->m_pData);
	Vector2* baseTexCoords =
		((Vector2*)GraphicsPipeline::s_quad_mesh->getTextureCoordBuffer()
			 ->m_rawBuffer->m_pData);

	// process current
	for (int j = 0; j < root->getDrawableCount(); j++) {
		UIDrawable* draw = &root->getDrawables()[j];
		for (int i = 0; i < 4; i++) {
			Matrix4 mat = Matrix4(1);
			Vector3 vert = baseVertices[i];

			vert = root->getTransformMatrix() * vert;
			vert.x = (vert.x * draw->size.x) + draw->localOffset.x;
			vert.y = (vert.y * draw->size.y) + draw->localOffset.y;

			m_vertices.push_back(vert.x);
			m_vertices.push_back(vert.y);
			m_vertices.push_back(0);
			m_textureCoords.push_back(baseTexCoords[i].x);
			m_textureCoords.push_back(baseTexCoords[i].y);
			m_colors.push_back(draw->color.x);
			m_colors.push_back(draw->color.y);
			m_colors.push_back(draw->color.z);
		}

		uint32_t i = m_vertexCount;
		m_indices.insert(m_indices.end(),
						 {i + 0, i + 1, i + 2, i + 2, i + 1, i + 3});

		m_vertexCount += 4;
	}
	// process children
	for (int i = 0; i < root->getChildren().size(); i++) {
		processTreeUI(root->getChildren()[i].get());
	}
}

std::shared_ptr<Mesh> UIManager::getUIMesh() {
	return m_mesh;
}

UIObject* UIManager::getRoot() {
	return m_root.get();
}

void UIManager::setUpdateUIFlag() {
	m_shouldUpdateUI = true;
}

}  // namespace bird
