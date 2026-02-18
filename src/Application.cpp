//
// Created by student on 10/5/23.
//

#include "Application.h"

#include "renders/Window.h"
#include "renders/opengl/GLPipeline.h"
#include "scene/2D/UIManager.h"
namespace bird {

Input* INPUT = nullptr;
ResourceManager* RESOURCE_MANAGER = nullptr;
Application* APPLICATION = nullptr;

Application::Application(Scene& scene) : m_scene(&scene) {}

Application::~Application() {}

void Application::init() {
	m_graphicsPipeline = new gl::GLPipeline();
	m_graphicsPipeline->init();
	INPUT = new Input(m_graphicsPipeline->getWindow().get());
	RESOURCE_MANAGER = new ResourceManager();
	APPLICATION = this;

	INPUT = new Input(m_graphicsPipeline->getWindow().get());
	m_uiManager = new UIManager();
	m_uiManager->init();

	m_scene->init();
}

void Application::run() {
	init();

	double lastTime = Window::getGLFWTime();
	float delta = 0.0f;
	uint16_t fps = 0;

	double secondTime = Window::getGLFWTime();
	while (!m_graphicsPipeline->getWindow()->shouldWindowClose()) {
		delta = (Window::getGLFWTime() - lastTime);
		lastTime = Window::getGLFWTime();
		m_graphicsPipeline->getCamera()->process(delta);
		if (m_graphicsPipeline->getCamera()->needsMatrixUpdate()) {
			m_graphicsPipeline->getCamera()->updateTransformationMatrix();
		}
		processScene(m_scene, delta);
		m_graphicsPipeline->renderRootScene(m_scene);
		fps++;
		if (secondTime + 1.0 < Window::getGLFWTime()) {
			std::cout << "FPS: " << fps << std::endl;
			fps = 0;
			secondTime = Window::getGLFWTime();
		}
		INPUT->tick();
		m_graphicsPipeline->getWindow()->pollWindow();
	}

	deinit();
}

void Application::processScene(Scene* scene, float delta) {
	m_uiManager->process(delta);
	for (auto& entity : scene->getEntities()) {
		entity->process(delta);
		if (entity->needsMatrixUpdate()) {
			entity->updateTransformationMatrix();
		}
	}
	for (auto& child : scene->getChildren()) {
		processScene(child, delta);
	}
}

void Application::deinit() {
	m_scene->deinit();

	m_uiManager->deinit();
	delete m_uiManager;
	delete INPUT;
	m_graphicsPipeline->cleanUp();
	delete m_graphicsPipeline;
}
void Application::setCamera(bird::Camera* camera) {
	m_graphicsPipeline->setCamera(camera);
}
bird::Camera* Application::getCamera() {
	return m_graphicsPipeline->getCamera();
}

GraphicsPipeline* Application::getGraphicsPipeline() {
	return m_graphicsPipeline;
}

UIManager* Application::getUIManager() {
	return m_uiManager;
}

}  // namespace bird
//
