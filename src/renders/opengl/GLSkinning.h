#ifndef BIRD_GL_SKINNING_H
#define BIRD_GL_SKINNING_H

#include <memory>
#include <vector>

#include "../generic/Buffer.h"
#include "../generic/GpuLayouts.h"
#include "../generic/RenderQueue.h"
#include "../generic/Shader.h"
#include "GLShader.h"

namespace bird::gl {

/**
 * The skinning uniform block (binding 4), shared by every pass.
 *
 * Uploads a palette only when it differs from the last one uploaded this
 * frame, so the meshes of one character (and its G-buffer, forward and
 * shadow draws) upload once per frame rather than once per draw.
 */
class GLSkinningBlock {
   public:
	GLSkinningBlock();

	/** Forgets the uploaded palette: its contents change every frame. */
	void beginFrame() { m_uploaded = nullptr; }

	/**
	 * Makes a palette current on binding 4.
	 * @throws std::invalid_argument when the palette is empty or exceeds
	 *         MAX_SKINNING_JOINTS entries.
	 */
	void upload(const std::vector<Matrix3x4>& palette);

   private:
	std::unique_ptr<Buffer<Matrix3x4>> m_block;
	const std::vector<Matrix3x4>* m_uploaded = nullptr;
};

/**
 * A pass's rigid and skinned programs: the same fragment stage behind two
 * vertex stages. prepare() binds whichever program an item needs, only
 * when it changes, and makes the item's palette current.
 */
class GLMeshPrograms {
   public:
	/**
	 * @throws std::invalid_argument when either program is null.
	 */
	GLMeshPrograms(std::shared_ptr<Shader> rigid, std::shared_ptr<Shader> skinned,
				   GLSkinningBlock& skinning);

	/** Call at the start of a pass: GL program state may have changed. */
	void reset() { m_bound = nullptr; }

	/** Binds the item's program and palette; @return the bound program. */
	GLShader& prepare(const RenderItem& item);

   private:
	std::shared_ptr<Shader> m_rigid;
	std::shared_ptr<Shader> m_skinned;
	GLSkinningBlock& m_skinning;
	GLShader* m_bound = nullptr;
};

}  // namespace bird::gl

#endif	// BIRD_GL_SKINNING_H
