//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"

#include <GL/glew.h>

#include <memory>

#include "../../Application.h"
#include "../../scene/2D/UIManager.h"
#include "../generic/Font.h"
#include "../generic/FrameBuffer.h"
#include "GLFrameBuffer.h"
#include "GLMesh.h"
#include "GLShader.h"
#include "GLUIRenderer.h"

namespace bird::gl {

GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {}

GLPipeline::~GLPipeline() {}

void GLPipeline::init() {
	m_pWindow = std::make_unique<Window>(WINDOW_WIDTH, WINDOW_HEIGHT,
										 "Bird Engine - Application");

	glewExperimental = true;
	int code = 0;
	if ((code = glewInit())) {
		std::cerr << glewGetErrorString(code) << std::endl;
		throw std::runtime_error("Couldn't initialize GLEW!");
	}

	initializeGraphicSpecifics();

	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glEnable(GL_DEBUG_OUTPUT);

	ui_shader = ShaderBuilder()
					.attachShaderFile("assets/shaders/ui.vert", VERTEX)
					.attachShaderFile("assets/shaders/ui.frag", FRAGMENT)
					.create();

	m_uiRenderer = std::make_unique<GLUIRenderer>();
	m_debugLineShader =
		ShaderBuilder()
			.attachShaderFile("assets/shaders/debug_line.vert", VERTEX)
			.attachShaderFile("assets/shaders/debug_line.frag", FRAGMENT)
			.create();
	m_debugRenderer = std::make_unique<GLDebugRenderer>();
	m_postProcessShader =
		ShaderBuilder()
			.attachShaderFile("assets/shaders/passthrough.vert", VERTEX)
			.attachShaderFile("assets/shaders/fxaa.frag", FRAGMENT)
			.create();
	createSceneFramebuffer();
}

void GLPipeline::createSceneFramebuffer() {
	// The builder reads WINDOW_WIDTH/HEIGHT, so this follows resize().
	FrameBufferBuilder builder;
	builder.addTextureAttachment(
		TextureBuilder(nullptr, WINDOW_WIDTH, WINDOW_HEIGHT, FORMAT_8RGBA)
			.build());
	builder.setShader(m_postProcessShader);
	builder.addOpts(FRAMEBUFFER_OPT_DEPTH | FRAMEBUFFER_OPT_STENCIL);
	m_framebuffers.clear();
	m_framebuffers.emplace_back(builder.build());
}

void GLPipeline::resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
						uint32_t uiWidth, uint32_t uiHeight) {
	GraphicsPipeline::resize(framebufferWidth, framebufferHeight, uiWidth,
							 uiHeight);
	createSceneFramebuffer();
}

void GLPipeline::renderRootScene(Scene* scene) {
	glEnable(GL_DEPTH_TEST);

	m_framebuffers.at(0).get()->bind(nullptr);

	glClearColor(0, 0, 0, 1);
	renderScene(scene);
	renderDebugLines();  // reuses the scene's global UBO (camera matrices)

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	((GLFrameBuffer*)m_framebuffers.at(0).get())->bindAttachments();

	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(((GLMesh*)s_quad_mesh.get())->getVAO());
	glDrawElements(GL_TRIANGLES, s_quad_mesh->getIndexBuffer()->getBufferSize(),
				   GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	auto& attachments = m_framebuffers.at(0)->getColorAttachments();
	for (int i = 0; i < attachments.size(); i++) {
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, 0);  // Unbind each texture
	}

	// The UI is drawn after post-processing so that FXAA and colour grading
	// leave the interface untouched.
	renderUI();
}

void GLPipeline::renderDebugLines() {
	if (m_debugDraw.empty()) return;
	((GLShader*)m_debugLineShader.get())->bind();
	m_debugRenderer->draw(m_debugDraw);
}

void GLPipeline::renderUI() {
	UIManager* uiManager = APPLICATION->getUIManager();
	const UIDrawList& drawList = uiManager->getDrawList();
	if (drawList.indices().empty()) return;

	((GLShader*)ui_shader.get())->bind();
	GlobalUBOData uiGlobals{};
	uiGlobals.perspective = m_orthoMatrix;
	uiGlobals.view = Matrix4(1.0f);
	m_globalUBO->update(&uiGlobals, 1, 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_uiRenderer->draw(drawList, uiManager->getFont()->getAtlas().get());

	glDisable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void GLPipeline::renderScene(Scene* scene) {
	// cull entities here

	// render entities

	GlobalUBOData global;
	MaterialUBOData matData;

	global.perspective = m_camera->getPerspectiveMatrix();
	global.view = m_camera->getViewMatrix();
	global.viewPos = Vector4(m_camera->getWorldPosition(), 0.0);

	m_globalUBO->update(&global, 1, 0);

	glClearColor(m_camera->getWorldEnvironment().clearColor.x,
				 m_camera->getWorldEnvironment().clearColor.y,
				 m_camera->getWorldEnvironment().clearColor.z, 1.0);

	MeshComponent* mesh = nullptr;
	WorldObject* obj = nullptr;
	for (int i = 0; i < m_meshes.size(); i++) {
		mesh = m_meshes[i];
		obj = mesh->world_object();
		for (int j = 0; j < mesh->getMeshes().size(); j++) {
			GLMesh* m = (GLMesh*)mesh->getMeshes()[j].get();
			const std::shared_ptr<Material> mat =
				mesh->getMeshes()[j]->getMaterial();
			GLShader* shader = (GLShader*)mat->getShader().get();
			shader->bind();
			glBindVertexArray(m->getVAO());

			const ObjectPushConstants object{obj->getTransformMatrix()};
			shader->setPushConstants(&object, sizeof(object));

			// std::cout << "Material:" << mat.get()->getDiffuseColor() <<
			// std::endl;

			matData.color = Vector4(mat.get()->getDiffuseColor(),
									mat.get()->getTextures().size() == 0);
			matData.specularColor = mat.get()->getSpecularColor();
			matData.ambientColor = mat.get()->getAmbientColor();
			matData.shininess = mat.get()->getShininess();

			m_materialUBO->update(&matData, 1, 0);

			// glGetBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MaterialUBOData),
			// &data); std::cout << "Material:" << data.color << std::endl;

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
	for (int i = 0; i < scene->getChildrenScenes().size(); i++) {
		// cull scene here
		renderScene(scene->getChildrenScenes()[i]);
	}
}

void GLPipeline::cleanUp() {
	m_debugRenderer.reset();
	m_pWindow.reset();
}

std::string GLPipeline::getName() const {
	return "OpenGL Pipeline";
}

}  // namespace bird::gl
