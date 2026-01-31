//
// Created by student on 10/5/23.
//

#include "Application.h"

#include <memory>

#include "physics/PhysicsPipeline.h"
#include "renders/Window.h"
#include "renders/opengl/GLPipeline.h"
namespace bird {

Input* INPUT = nullptr;
ResourceManager* RESOURCE_MANAGER = nullptr;
Application* APPLICATION = nullptr;

Application::Application(Scene& scene) : m_scene(&scene) {}

Application::~Application() = default;

void Application::init() {
	m_graphicsPipeline = std::make_unique<gl::GLPipeline>();
	m_physicsPipeline = std::make_unique<PhysicsPipeline>();

	m_graphicsPipeline->init();
	m_physicsPipeline->init();

	INPUT = new Input(m_graphicsPipeline->getWindow().get());
	RESOURCE_MANAGER = new ResourceManager();
	APPLICATION = this;

	INPUT = new Input(m_graphicsPipeline->getWindow().get());

	INPUT->addAction(Action("move_forward").addKey(KEY_W));
	INPUT->addAction(Action("move_backward").addKey(KEY_S));
	INPUT->addAction(Action("move_left").addKey(KEY_A));
	INPUT->addAction(Action("move_right").addKey(KEY_D));
	INPUT->addAction(Action("move_up").addKey(KEY_SPACE));
	INPUT->addAction(Action("move_down").addKey(KEY_LEFT_SHIFT));

	INPUT->addAction(Action("look_up").addKey(KEY_UP));
	INPUT->addAction(Action("look_down").addKey(KEY_DOWN));
	INPUT->addAction(Action("look_left").addKey(KEY_LEFT));
	INPUT->addAction(Action("look_right").addKey(KEY_RIGHT));

	m_scene->init();
}

void Application::run() {
	init();

	double lastTime = Window::getGLFWTime();
	double physicsTime = Window::getGLFWTime();
	float delta = 0.0f;
	float delta2 = 0.0f;
	float fixedStep = 1.0 / 60.0;  // physics step time
	double accumulator = 0.0;

	while (!m_graphicsPipeline->getWindow()->shouldWindowClose()) {
		delta = (Window::getGLFWTime() - lastTime);
		lastTime = Window::getGLFWTime();

		if (delta > 0.25) delta = 0.25;
		accumulator += delta;
		while (accumulator >= fixedStep) {
			m_physicsPipeline->update(fixedStep);

			m_graphicsPipeline->getCamera()->process(delta);
			if (m_graphicsPipeline->getCamera()->needsMatrixUpdate()) {
				m_graphicsPipeline->getCamera()->updateTransformationMatrix();
			}
			processScene(m_scene, delta);

			INPUT->tick();

			accumulator -= fixedStep;
		}

		m_graphicsPipeline->renderRootScene(m_scene);
		m_graphicsPipeline->getWindow()->pollWindow();
	}

	deinit();
}

void Application::processScene(Scene* scene, float delta) {
	for (auto& entity : scene->getEntities()) {
		entity->process(delta);
		if (entity->needsMatrixUpdate()) {
			entity->updateTransformationMatrix();
		}
	}
	for (auto& child : scene->getChildren()) {
		processScene(child.get(), delta);
	}
}

void Application::deinit() {
	m_scene->deinit();

	delete INPUT;
	m_physicsPipeline->cleanup();
	m_graphicsPipeline->cleanUp();
}
void Application::setCamera(bird::Camera* camera) {
	m_graphicsPipeline->setCamera(camera);
}
bird::Camera* Application::getCamera() {
	return m_graphicsPipeline->getCamera();
}

GraphicsPipeline* Application::getGraphicsPipeline() {
	return m_graphicsPipeline.get();
}

PhysicsPipeline* Application::getPhysicsPipeline() {
	return m_physicsPipeline.get();
}

}  // namespace bird
//
