//
// Created by student on 2/7/23.
//

#ifndef BIRD_GLSHADER_H
#define BIRD_GLSHADER_H

#include "../generic/Shader.h"
#include "GLBuffer.h"

namespace bird::gl {

class GLShader : public Shader {
   public:
	GLShader(std::vector<std::pair<std::string, ShaderPipeline>> shaderFiles);
	~GLShader() override;

	void loadPerspectiveMatrix(Matrix4 perspectiveMatrix) override;
	void loadViewMatrix(Matrix4 viewMatrix) override;
	void loadModelMatrix(Matrix4 modelMatrix) override;

	void bind() const;
	void unbind() const;

   private:
	uint32_t m_programId;
	// std::vector<GLBuffer> m_uniformBuffers;
};

}  // namespace bird::gl

#endif	// BIRD_GLSHADER_H
