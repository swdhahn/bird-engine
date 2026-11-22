#include "GLParticleRenderer.h"

#include "GLApi.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "../../util/ResourceManager.h"
#include "../generic/GpuLayouts.h"
#include "GLDrawCommands.h"
#include "GLShader.h"
#include "GLTexture.h"

namespace bird::gl {

namespace {

void setParticleAttrib(GLuint location, GLint components, size_t offset) {
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE,
						  sizeof(ParticleQuadVertex),
						  reinterpret_cast<void*>(offset));
}

}  // namespace

GLParticleRenderer::GLParticleRenderer(ResourceManager& resources) {
	m_shader = resources.loadShader("assets/shaders/particle.vert",
									"assets/shaders/particle.frag");

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	m_vertices = std::make_unique<GLBuffer>(
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC | BUFFER_ARRAY),
		ParticleEmitter::kDefaultCapacity * 4, sizeof(ParticleQuadVertex));
	m_vertices->initialize();

	setParticleAttrib(0, 3, offsetof(ParticleQuadVertex, position));
	setParticleAttrib(1, 2, offsetof(ParticleQuadVertex, uv));
	setParticleAttrib(2, 4, offsetof(ParticleQuadVertex, color));

	m_indices = std::make_unique<GLBuffer>(
		static_cast<BufferMode>(BUFFER_STAGED | BUFFER_DYNAMIC |
								BUFFER_ELEMENT_ARRAY),
		ParticleEmitter::kDefaultCapacity * 6, sizeof(uint32_t));
	m_indices->initialize();

	glBindVertexArray(0);
	ensureIndexCapacity(ParticleEmitter::kDefaultCapacity);
}

GLParticleRenderer::~GLParticleRenderer() {
	if (m_fallbackWhiteTex != 0) {
		glDeleteTextures(1, &m_fallbackWhiteTex);
		m_fallbackWhiteTex = 0;
	}
	m_vertices.reset();
	m_indices.reset();
	if (m_vao != 0) {
		glDeleteVertexArrays(1, &m_vao);
		m_vao = 0;
	}
}

void GLParticleRenderer::ensureIndexCapacity(size_t quadCount) {
	if (quadCount <= m_indexedQuadCapacity) return;

	size_t newCap = std::max<size_t>(ParticleEmitter::kDefaultCapacity,
									 m_indexedQuadCapacity);
	while (newCap < quadCount) newCap *= 2;

	std::vector<uint32_t> indices(newCap * 6);
	for (size_t q = 0; q < newCap; q++) {
		const uint32_t v = static_cast<uint32_t>(q * 4);
		indices[q * 6 + 0] = v + 0;
		indices[q * 6 + 1] = v + 1;
		indices[q * 6 + 2] = v + 2;
		indices[q * 6 + 3] = v + 0;
		indices[q * 6 + 4] = v + 2;
		indices[q * 6 + 5] = v + 3;
	}

	glBindVertexArray(m_vao);
	m_indices->update(indices.data(), indices.size(), 0);
	glBindVertexArray(0);
	m_indexedQuadCapacity = newCap;
}

uint32_t GLParticleRenderer::ensureFallbackWhiteTexture() {
	if (m_fallbackWhiteTex != 0) return m_fallbackWhiteTex;
	const uint8_t white[4] = {255, 255, 255, 255};
	glGenTextures(1, &m_fallbackWhiteTex);
	glBindTexture(GL_TEXTURE_2D, m_fallbackWhiteTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
				 GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	return m_fallbackWhiteTex;
}

void GLParticleRenderer::draw(const std::vector<ParticleEmitter*>& emitters,
							  const Matrix4& cameraView, GLRenderState& state) {
	if (emitters.empty()) return;

	// Extract world-space camera basis vectors from the rows of the view matrix.
	const Vector3 cameraRight =
		glm::normalize(Vector3(cameraView[0][0], cameraView[1][0], cameraView[2][0]));
	const Vector3 cameraUp =
		glm::normalize(Vector3(cameraView[0][1], cameraView[1][1], cameraView[2][1]));
	const Vector3 cameraForward = -glm::normalize(
		Vector3(cameraView[0][2], cameraView[1][2], cameraView[2][2]));

	state.setDepthTest(true);
	state.setDepthWrite(false);
	state.setDepthFunc(GLDepthFunc::LessEqual);
	state.setCull(GLCull::None);

	auto& glShader = static_cast<GLShader&>(*m_shader);
	glShader.bind();
	const uint32_t fallbackTex = ensureFallbackWhiteTexture();

	for (ParticleEmitter* emitter : emitters) {
		if (emitter == nullptr || emitter->getActiveCount() == 0) continue;

		const size_t quadCount = emitter->getActiveCount();
		ensureIndexCapacity(quadCount);

		const size_t vertexCount =
			emitter->buildBillboardQuads(cameraRight, cameraUp, cameraForward);
		if (vertexCount == 0) continue;

		state.setBlend(emitter->getBlendMode() == ParticleBlendMode::Additive
						   ? GLBlend::AdditiveAlpha
						   : GLBlend::Alpha);

		const auto& tex = emitter->getTexture();
		glActiveTexture(GL_TEXTURE0);
		if (tex != nullptr) {
			glBindTexture(
				GL_TEXTURE_2D,
				const_cast<GLTexture&>(static_cast<const GLTexture&>(*tex))
					.getTextureID());
		} else {
			glBindTexture(GL_TEXTURE_2D, fallbackTex);
		}

		const ParticlePushConstants push{
			Vector4(tex != nullptr ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f)};
		glShader.setPushConstants(&push, sizeof(push));

		glBindVertexArray(m_vao);
		m_vertices->update(
			const_cast<ParticleQuadVertex*>(emitter->getQuadVertices()),
			vertexCount, 0);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(quadCount * 6),
					   GL_UNSIGNED_INT, nullptr);
	}

	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace bird::gl
