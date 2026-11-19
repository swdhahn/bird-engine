#include "GLUIRenderer.h"

#include <GL/glew.h>

#include <cstddef>

#include "../../scene/2D/UIDrawList.h"
#include "GLTexture.h"

namespace bird::gl {

namespace {

void attribute(GLuint location, GLint components, size_t offset) {
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE,
						  sizeof(UIVertex), reinterpret_cast<void*>(offset));
}

}  // namespace

GLUIRenderer::GLUIRenderer() {
	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);
	m_vertices = std::make_unique<GLBuffer>(
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC | BUFFER_ARRAY),
		1, sizeof(UIVertex));
	m_vertices->initialize();
	// Locations match assets/shaders/ui.vert.
	attribute(0, 2, offsetof(UIVertex, position));
	attribute(1, 4, offsetof(UIVertex, color));
	attribute(2, 2, offsetof(UIVertex, uv));
	attribute(3, 4, offsetof(UIVertex, clip));
	attribute(4, 4, offsetof(UIVertex, shape));
	attribute(5, 4, offsetof(UIVertex, style));
	attribute(6, 4, offsetof(UIVertex, borderColor));
	m_indices = std::make_unique<GLBuffer>(
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
								BUFFER_ELEMENT_ARRAY),
		1, sizeof(uint32_t));
	m_indices->initialize();  // binds into the VAO
	glBindVertexArray(0);
}

GLUIRenderer::~GLUIRenderer() {
	m_vertices.reset();
	m_indices.reset();
	glDeleteVertexArrays(1, &m_vao);
}

void GLUIRenderer::draw(const UIDrawList& list, const Texture* atlas) {
	if (list.indices().empty()) return;
	// Bind the VAO first so the element buffer update targets it.
	glBindVertexArray(m_vao);
	m_vertices->update(const_cast<UIVertex*>(list.vertices().data()),
					   list.vertices().size(), 0);
	m_indices->update(const_cast<uint32_t*>(list.indices().data()),
					  list.indices().size(), 0);
	glActiveTexture(GL_TEXTURE0);
	for (const UIDrawCommand& command : list.commands()) {
		const Texture* texture = command.texture ? command.texture : atlas;
		if (!texture) continue;
		const_cast<GLTexture*>(static_cast<const GLTexture*>(texture))->bind();
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(command.indexCount),
					   GL_UNSIGNED_INT,
					   reinterpret_cast<void*>(command.firstIndex * sizeof(uint32_t)));
	}
	glBindVertexArray(0);
}

}  // namespace bird::gl
