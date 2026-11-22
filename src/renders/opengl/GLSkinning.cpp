#include "GLSkinning.h"

#include <stdexcept>

#include "../../animation/skeletal/Skeleton.h"

namespace bird::gl {

static_assert(Skeleton::kMaxJoints == MAX_SKINNING_JOINTS,
			  "skeleton joint limit must match the skinning block");

GLSkinningBlock::GLSkinningBlock() {
	auto identity = std::make_unique<Matrix3x4[]>(MAX_SKINNING_JOINTS);
	for (uint32_t i = 0; i < MAX_SKINNING_JOINTS; ++i) {
		identity[i] = Matrix3x4(Vector4(1, 0, 0, 0), Vector4(0, 1, 0, 0),
								Vector4(0, 0, 1, 0));
	}
	m_block = BufferBuilder<Matrix3x4>(BUFFER_UNIFORM | BUFFER_STAGED |
									   BUFFER_DYNAMIC)
				  .data(std::move(identity), MAX_SKINNING_JOINTS)
				  .bindingPoint(UBO_BINDING_SKINNING)
				  .build();
}

void GLSkinningBlock::upload(const std::vector<Matrix3x4>& palette) {
	if (palette.empty() || palette.size() > MAX_SKINNING_JOINTS) {
		throw std::invalid_argument(
			"GLSkinningBlock::upload: palette must hold 1.." +
			std::to_string(MAX_SKINNING_JOINTS) + " joints");
	}
	if (&palette != m_uploaded) {
		// Joints past the palette keep stale data; vertices never index
		// them because skin streams are validated against the skeleton.
		m_block->update(const_cast<Matrix3x4*>(palette.data()), palette.size(),
						0);
		m_uploaded = &palette;
	}
	m_block->bindToBindingPoint();
}

GLMeshPrograms::GLMeshPrograms(std::shared_ptr<Shader> rigid,
							   std::shared_ptr<Shader> skinned,
							   GLSkinningBlock& skinning)
	: m_rigid(std::move(rigid)),
	  m_skinned(std::move(skinned)),
	  m_skinning(skinning) {
	if (!m_rigid || !m_skinned) {
		throw std::invalid_argument("GLMeshPrograms: both programs required");
	}
}

GLShader& GLMeshPrograms::prepare(const RenderItem& item) {
	auto& program =
		static_cast<GLShader&>(item.isSkinned() ? *m_skinned : *m_rigid);
	if (&program != m_bound) {
		program.bind();
		m_bound = &program;
	}
	if (item.isSkinned()) m_skinning.upload(*item.skinPalette);
	return program;
}

}  // namespace bird::gl
