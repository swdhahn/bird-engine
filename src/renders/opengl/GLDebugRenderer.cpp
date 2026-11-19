#include "GLDebugRenderer.h"

#include <GL/glew.h>

#include <cstddef>

#include "../DebugDraw.h"

namespace bird::gl {

GLDebugRenderer::LineBuffer::LineBuffer() {
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	vertices = std::make_unique<GLBuffer>(
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC | BUFFER_ARRAY), 1,
		sizeof(DebugVertex));
	vertices->initialize();
	// Locations match assets/shaders/debug_line.vert.
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
						  reinterpret_cast<void*>(offsetof(DebugVertex, position)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(DebugVertex),
						  reinterpret_cast<void*>(offsetof(DebugVertex, color)));
	glBindVertexArray(0);
}

GLDebugRenderer::LineBuffer::~LineBuffer() {
	vertices.reset();
	glDeleteVertexArrays(1, &vao);
}

void GLDebugRenderer::LineBuffer::upload(const void* data, size_t vertexCount) {
	glBindVertexArray(vao);
	vertices->update(const_cast<void*>(data), vertexCount, 0);
	glBindVertexArray(0);
	count = vertexCount;
}

void GLDebugRenderer::LineBuffer::draw() const {
	if (count == 0) return;
	glBindVertexArray(vao);
	glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(count));
	glBindVertexArray(0);
}

void GLDebugRenderer::draw(const DebugDrawList& list) {
	// Free GPU copies of layers the list no longer has.
	for (auto it = m_layers.begin(); it != m_layers.end();) {
		it = list.cachedLayers().count(it->first) ? std::next(it) : m_layers.erase(it);
	}
	if (list.empty()) return;

	if (list.drawOnTop) glDisable(GL_DEPTH_TEST);
	for (const auto& [key, layer] : list.cachedLayers()) {
		if (!layer.used) continue;
		std::unique_ptr<LineBuffer>& buffer = m_layers[key];
		if (!buffer) buffer = std::make_unique<LineBuffer>();
		if (buffer->version != layer.version) {
			buffer->upload(layer.vertices.data(), layer.vertices.size());
			buffer->version = layer.version;
		}
		buffer->draw();
	}
	if (!list.vertices().empty()) {
		if (!m_transient) m_transient = std::make_unique<LineBuffer>();
		m_transient->upload(list.vertices().data(), list.vertices().size());
		m_transient->draw();
	}
	glEnable(GL_DEPTH_TEST);
}

}  // namespace bird::gl
