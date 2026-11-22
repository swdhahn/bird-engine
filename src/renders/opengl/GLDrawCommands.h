#ifndef BIRD_GL_DRAW_COMMANDS_H
#define BIRD_GL_DRAW_COMMANDS_H

#include "../generic/Material.h"
#include "../generic/Mesh.h"
#include "../generic/RenderQueue.h"
#include "GLRenderState.h"
#include "GLShader.h"
#include "GLSkinning.h"

namespace bird::gl {

/**
 * Binds a material's uniform block (binding 1) and its textures to sampler
 * units 0..4, and applies its face culling.
 */
void bindMaterial(Material& material, GLRenderState& state);

/** Issues the indexed draw for a mesh using its own VAO. */
void drawMesh(const Mesh& mesh);

/**
 * Draws render items, binding the rigid or skinned program each needs and
 * rebinding a material only when it changes between consecutive items.
 */
void drawItems(const std::vector<RenderItem>& items, GLMeshPrograms& programs,
			   GLRenderState& state);

/** Binds a texture to a sampler unit. */
void bindTextureUnit(uint32_t unit, const Texture& texture);

/** Draws the shared full-screen quad. */
void drawFullscreenQuad();

}  // namespace bird::gl

#endif	// BIRD_GL_DRAW_COMMANDS_H
