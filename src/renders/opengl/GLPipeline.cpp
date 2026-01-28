//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"

#include <memory>

#include "../generic/FrameBuffer.h"
#include "GLFrameBuffer.h"
#include "GLMesh.h"

namespace bird::gl {

GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {}

GLPipeline::~GLPipeline() {}

void GLPipeline::init() {
	m_pWindow = std::make_unique<Window>(WINDOW_WIDTH, WINDOW_HEIGHT, "Window");

	glewExperimental = true;
	int code = 0;
	if ((code = glewInit())) {
		std::cerr << glewGetErrorString(code) << std::endl;
		throw std::runtime_error("Couldn't initialize GLEW!");
	}

	initializeGraphicSpecifics();

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEBUG_OUTPUT);

	FrameBufferBuilder builder;
	builder.addTextureAttachment(
		TextureBuilder(nullptr, WINDOW_WIDTH, WINDOW_HEIGHT, FORMAT_8RGBA)
			.build());
	builder.setShader(
		ShaderBuilder()
			.attachShaderFile("assets/shaders/passthrough.vert", VERTEX)
			.attachShaderFile("assets/shaders/passthrough.frag", FRAGMENT)
			.create());
	builder.addOpts(FRAMEBUFFER_OPT_DEPTH | FRAMEBUFFER_OPT_STENCIL);
	m_framebuffers.emplace_back(builder.build());
}

void GLPipeline::renderRootScene(const bird::Scene* scene) {
	glEnable(GL_DEPTH_TEST);
	glClearColor(1, 0, 0, 1);

	m_framebuffers.at(0).get()->bind(nullptr);
	renderScene(scene);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	((GLFrameBuffer*)m_framebuffers.at(0).get())->bindAttachments();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);	 // <--- Likely Culprit
	glDisable(GL_STENCIL_TEST);	 // <--- Possible Culprit
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE,
				GL_TRUE);  // <--- Ensure we can write
	glDepthMask(GL_TRUE);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindVertexArray(((GLMesh*)m_quad_mesh.get())->getVAO());
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glActiveTexture(GL_TEXTURE0);

	glfwSwapBuffers(m_pWindow->getGLFWWindow());
}

void GLPipeline::renderScene(const Scene* scene) {
	// cull entities here

	// render entities

	GlobalUBOData global;
	MaterialUBOData matData;

	global.perspective = m_camera->getPerspectiveMatrix();
	global.view = m_camera->getTransformMatrix();
	global.viewPos = Vector4(m_camera->getWorldPosition(), 0.0);

	MeshComponent* mesh = nullptr;
	WorldObject* obj = nullptr;
	for (int i = 0; i < m_meshes.size(); i++) {
		mesh = m_meshes[i];
		obj = mesh->getParent();
		for (int j = 0; j < mesh->getMeshes().size(); j++) {
			GLMesh* m = (GLMesh*)mesh->getMeshes()[j].get();
			const std::shared_ptr<Material> mat =
				mesh->getMeshes()[j]->getMaterial();
			GLShader* shader = (GLShader*)mat->getShader().get();
			shader->bind();
			glBindVertexArray(m->getVAO());

			global.model = obj->getTransformMatrix();

			m_globalUBO->update(&global, 1, 0);

			// std::cout << "Material:" << mat.get()->getDiffuseColor() <<
			// std::endl;

			matData.color = mat.get()->getDiffuseColor();
			matData.specularColor = mat.get()->getSpecularColor();
			matData.ambientColor = mat.get()->getAmbientColor();
			matData.shininess = mat.get()->getShininess();

			m_materialUBO->update(&matData, 1, 0);

			// glGetBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUBOData),
			// &data); std::cout << "Material:" << data.color << std::endl;

			// TODO: Uniform buffer objects and textures
			// shader->loadPerspectiveMatrix(m_camera->getPerspectiveMatrix());
			// shader->loadViewMatrix(m_camera->getTransformMatrix());
			// shader->loadModelMatrix(e->getTransformMatrix());

			if (mat != nullptr) {
				for (int k = 0; k < mat->getTextures().size(); k++) {
					glActiveTexture(GL_TEXTURE0 + k);
					((GLTexture*)mat->getTextures()[k].get())->bind();
				}
			}
			glDrawElements(
				GL_TRIANGLES,
				mesh->getMeshes()[j]->getIndexBuffer()->getBufferSize(),
				GL_UNSIGNED_INT, 0);
		}
	}
	for (int i = 0; i < scene->getChildren().size(); i++) {
		// cull scene here
		renderScene(scene->getChildren()[i]);
	}
}

void GLPipeline::cleanUp() {
	m_pWindow.reset();
}

std::string GLPipeline::getName() const {
	return "OpenGL Pipeline";
}

}  // namespace bird::gl
