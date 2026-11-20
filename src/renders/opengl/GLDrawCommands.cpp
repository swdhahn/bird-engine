#include "GLDrawCommands.h"

#include <GL/glew.h>

#include "../GraphicsPipeline.h"
#include "GLMesh.h"
#include "GLTexture.h"

namespace bird::gl {

void bindTextureUnit(uint32_t unit, const Texture& texture) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D,
				  const_cast<GLTexture&>(static_cast<const GLTexture&>(texture))
					  .getTextureID());
}

void bindMaterial(Material& material, GLRenderState& state) {
	material.acquireUniformBuffer()->bindToBindingPoint();
	for (uint32_t slot = 0; slot < static_cast<uint32_t>(TextureSlot::Count);
		 slot++) {
		const auto& texture = material.getTexture(static_cast<TextureSlot>(slot));
		if (texture) bindTextureUnit(slot, *texture);
	}
	state.setCull(material.isDoubleSided() ? GLCull::None : GLCull::Back);
}

void drawMesh(const Mesh& mesh) {
	const auto& glMesh = static_cast<const GLMesh&>(mesh);
	glBindVertexArray(glMesh.getVAO());
	glDrawElements(
		GL_TRIANGLES,
		static_cast<GLsizei>(
			const_cast<Mesh&>(mesh).getIndexBuffer()->getBufferSize()),
		GL_UNSIGNED_INT, nullptr);
}

void drawItems(const std::vector<RenderItem>& items, GLMeshPrograms& programs,
			   GLRenderState& state) {
	const Material* bound = nullptr;
	for (const RenderItem& item : items) {
		if (item.material != bound) {
			bindMaterial(*item.material, state);
			bound = item.material;
		}
		GLShader& shader = programs.prepare(item);
		const ObjectPushConstants object{item.model};
		shader.setPushConstants(&object, sizeof(object));
		drawMesh(*item.mesh);
	}
	glBindVertexArray(0);
}

void drawFullscreenQuad() {
	drawMesh(*GraphicsPipeline::s_quad_mesh);
	glBindVertexArray(0);
}

}  // namespace bird::gl
