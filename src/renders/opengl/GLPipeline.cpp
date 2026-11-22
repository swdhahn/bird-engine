//
// Created by Seth Hahn on 11/17/22.
//

#include "GLPipeline.h"

#include "GLApi.h"

#include <cstdlib>
#include <fstream>
#include <memory>
#include <vector>

#include "../../Application.h"
#include "../../platform/Platform.h"
#include "../../scene/2D/UIManager.h"
#include "../generic/Font.h"
#include "../generic/FrameBuffer.h"
#include "GLCapabilities.h"
#include "GLFrameBuffer.h"
#include "GLMesh.h"
#include "GLShader.h"
#include "GLUIRenderer.h"

namespace bird::gl {

GLPipeline::GLPipeline() : GraphicsPipeline(GRAPHICS_PIPELINE_OPENGL) {}

GLPipeline::~GLPipeline() {}

void GLPipeline::init() {
	m_pWindow = platform::createWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
									   "Bird Engine - Application");

	loadGLFunctions();
	const GLCapabilities& caps = GLCapabilities::current();

	initializeGraphicSpecifics();

	glDisable(GL_CULL_FACE);

	if (caps.debugOutput) glEnable(GL_DEBUG_OUTPUT);

	if (RESOURCE_MANAGER == nullptr) {
		throw std::logic_error(
			"GLPipeline::init: construct the ResourceManager first.");
	}
	ResourceManager& resources = *RESOURCE_MANAGER;
	ui_shader = resources.loadShader("assets/shaders/ui.vert",
									 "assets/shaders/ui.frag");

	m_uiRenderer = std::make_unique<GLUIRenderer>();
	m_debugLineShader = resources.loadShader("assets/shaders/debug_line.vert",
											 "assets/shaders/debug_line.frag");
	m_debugRenderer = std::make_unique<GLDebugRenderer>();
	m_postProcessShader = resources.loadShader(
		"assets/shaders/passthrough.vert", "assets/shaders/fxaa.frag");

	auto lights = std::make_unique<LightsUBOData[]>(1);
	m_lightsBlock = BufferBuilder<LightsUBOData>(BUFFER_UNIFORM |
												 BUFFER_STAGED | BUFFER_DYNAMIC)
						.data(std::move(lights), 1)
						.bindingPoint(UBO_BINDING_LIGHTS)
						.build();

	m_skinning = std::make_unique<GLSkinningBlock>();
	m_shadowPass = std::make_unique<GLShadowPass>(resources, ShadowSettings{},
												  *m_skinning);
	m_gBufferPass = std::make_unique<GLGBufferPass>(resources, *m_skinning);
	m_lightingPass = std::make_unique<GLLightingPass>(resources);
	m_forwardPass = std::make_unique<GLForwardPass>(resources, *m_skinning);
	m_postProcessPass = std::make_unique<GLPostProcessPass>(resources);
	createRenderTargets();
}

void GLPipeline::createRenderTargets() {
	m_targets = std::make_unique<GLRenderTargets>(WINDOW_WIDTH, WINDOW_HEIGHT,
												  m_postProcessShader);
}

void GLPipeline::resize(uint32_t framebufferWidth, uint32_t framebufferHeight,
						uint32_t uiWidth, uint32_t uiHeight) {
	GraphicsPipeline::resize(framebufferWidth, framebufferHeight, uiWidth,
							 uiHeight);
	createRenderTargets();
}

void GLPipeline::renderRootScene(Scene* scene) {
	// Rendering draws every registered mesh once; the scene tree only
	// decides registration, so nested scenes are not traversed here.
	(void)scene;
	m_state.invalidate();
	renderWorld();
	// The UI is drawn after post-processing so that FXAA and tone mapping
	// leave the interface untouched.
	renderUI();
	m_state.invalidate();
	captureFrameIfRequested();
	m_frameIndex++;
}

void GLPipeline::captureFrameIfRequested() {
	static const char* path = std::getenv("BIRD_CAPTURE");
	if (path == nullptr) return;
	static const uint64_t target = [] {
		const char* frame = std::getenv("BIRD_CAPTURE_FRAME");
		return frame ? std::strtoull(frame, nullptr, 10) : 60ull;
	}();
	if (m_frameIndex != target) return;

	const uint32_t width = WINDOW_WIDTH;
	const uint32_t height = WINDOW_HEIGHT;
	// RGBA is the one read format every desktop, ES and WebGL context
	// accepts for the default framebuffer.
	std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
				 pixels.data());

	std::ofstream out(path, std::ios::binary);
	out << "P6\n" << width << " " << height << "\n255\n";
	// GL rows run bottom to top; PPM rows run top to bottom.
	std::vector<char> rgbRow(static_cast<size_t>(width) * 3);
	for (uint32_t row = 0; row < height; row++) {
		const uint8_t* source =
			pixels.data() + static_cast<size_t>(height - 1 - row) * width * 4;
		for (uint32_t x = 0; x < width; x++) {
			rgbRow[x * 3 + 0] = static_cast<char>(source[x * 4 + 0]);
			rgbRow[x * 3 + 1] = static_cast<char>(source[x * 4 + 1]);
			rgbRow[x * 3 + 2] = static_cast<char>(source[x * 4 + 2]);
		}
		out.write(rgbRow.data(), static_cast<std::streamsize>(rgbRow.size()));
	}
	std::cout << "Captured frame " << target << " to " << path << std::endl;
}

void GLPipeline::renderWorld() {
	m_skinning->beginFrame();  // palettes were rewritten since last frame
	GlobalUBOData global = buildGlobalData();
	m_globalUBO->update(&global, 1, 0);
	m_globalUBO->bindToBindingPoint();

	const Frustum frustum =
		Frustum::fromViewProjection(global.perspective * global.view);
	buildRenderQueue(frustum);

	std::vector<LightSnapshot> lights = snapshotLights();
	ShadowCameraInfo cameraInfo;
	cameraInfo.inverseView = global.inverseView;
	cameraInfo.position = Vector3(global.viewPos);
	cameraInfo.fovY = m_camera->getFieldOfView();
	cameraInfo.aspect = global.screenSize.x / global.screenSize.y;
	cameraInfo.nearPlane = m_camera->getNearPlane();
	cameraInfo.shadowDistance = std::min(m_camera->getFarPlane(), 120.0f);
	const ShadowPlan shadowPlan = planShadows(lights, cameraInfo, frustum,
											  m_shadowPass->settings());
	m_shadowPass->execute(shadowPlan, m_shadowCasters, m_state);

	LightsUBOData lightBlock = buildLightsBlock(
		lights, selectForwardLights(lights, cameraInfo.position, frustum,
									MAX_FORWARD_LIGHTS));
	m_lightsBlock->update(&lightBlock, 1, 0);
	m_lightsBlock->bindToBindingPoint();
	m_shadowPass->bindForSampling();

	const WorldEnvironment& env = m_camera->getWorldEnvironment();
	m_gBufferPass->execute(m_renderQueue.bucket(RenderBucket::Deferred),
						   *m_targets, env.clearColor, m_state);
	m_targets->gBuffer().blitDepthTo(m_targets->hdr());
	m_lightingPass->execute(lights, frustum, *m_targets, m_state);
	m_forwardPass->execute(m_renderQueue, *m_targets, m_state);

	m_targets->hdr().bindForDrawing();
	renderDebugLines();
	m_postProcessPass->execute(*m_targets, WINDOW_WIDTH, WINDOW_HEIGHT,
							   m_state);
}

void GLPipeline::renderDebugLines() {
	if (m_debugDraw.empty()) return;
	m_state.setDepthTest(true);
	m_state.setDepthWrite(true);
	m_state.setDepthFunc(GLDepthFunc::Less);
	m_state.setBlend(GLBlend::None);
	m_state.setCull(GLCull::None);
	((GLShader*)m_debugLineShader.get())->bind();
	m_debugRenderer->draw(m_debugDraw);
	// The debug renderer changes GL state outside the cache.
	m_state.invalidate();
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

void GLPipeline::cleanUp() {
	m_debugRenderer.reset();
	m_postProcessPass.reset();
	m_forwardPass.reset();
	m_lightingPass.reset();
	m_gBufferPass.reset();
	m_shadowPass.reset();
	m_skinning.reset();	 // after the passes that reference it
	m_targets.reset();
	m_lightsBlock.reset();
	m_pWindow.reset();
}

std::string GLPipeline::getName() const {
	return "OpenGL Pipeline";
}

}  // namespace bird::gl
