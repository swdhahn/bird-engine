//
// Created by student on 10/5/23.
//

#include "Application.h"

#include "renders/Window.h"
#include "renders/opengl/GLPipeline.h"
#include "scene/2D/UIManager.h"
#include "scene/Scene.h"
#include "scene/components/Component.h"
#include "scene/components/ProcessScript.h"
#include "scene/components/Script.h"

namespace bird {

Input* INPUT = nullptr;
ResourceManager* RESOURCE_MANAGER = nullptr;
Application* APPLICATION = nullptr;

Application::Application() {}

Application::~Application() {}

void Application::init(Script<Scene>* script) {
	m_graphicsPipeline = new gl::GLPipeline();
	m_graphicsPipeline->init();
	RESOURCE_MANAGER = new ResourceManager();
	RESOURCE_MANAGER->init();
	APPLICATION = this;

	INPUT = new Input(m_graphicsPipeline->getWindow().get());
	m_uiManager = new UIManager();
	m_uiManager->init(RESOURCE_MANAGER->loadFont(UIManager::kDefaultFontPath),
					  m_graphicsPipeline->getWindow().get());
	// The compositor may already have resized the window (e.g. tiling).
	handleResize();

	// Before the scene: bodies are created as soon as objects are added.
	m_physicsServer = _own_mk<PhysicsServer>();

	m_scene = new Scene();
	m_scene->addComponent(std::unique_ptr<Script<Scene>>(script));
	m_scene->init(nullptr);

	run();
}

void Application::run() {
	double lastTime = Window::getGLFWTime();
	float delta = 0.0f;
	uint16_t fps = 0;

	double secondTime = Window::getGLFWTime();
	while (!m_graphicsPipeline->getWindow()->shouldWindowClose()) {
		delta = (Window::getGLFWTime() - lastTime);
		lastTime = Window::getGLFWTime();

		if (m_graphicsPipeline->getCamera()->needsMatrixUpdate()) {
			m_graphicsPipeline->getCamera()->updateTransformationMatrix();
		}

		// Debug lines are rebuilt every frame (physics first, then scripts).
		m_graphicsPipeline->getDebugDraw().clear();

		// PHYSICS SECTION: zero or more fixed steps
		const int physicsSteps = m_physicsClock.advance(delta);
		const float fixedStep = m_physicsClock.getStep();
		for (int step = 0; step < physicsSteps; step++) {
			for (size_t i = 0; i < m_processableScripts.size(); i++) {
				m_processableScripts[i]->physicsProcess(fixedStep);
			}
			m_physicsServer->step(fixedStep);
		}
		if (m_physicsDebugDraw) {
			Camera* camera = m_graphicsPipeline->getCamera();
			const Vector3 eye = camera ? camera->getWorldPosition() : Vector3(0.0f);
			for (PhysicsWorld* world : m_physicsServer->getWorlds()) {
				world->debugDraw(m_graphicsPipeline->getDebugDraw(),
								 m_physicsDebugSettings, eye);
			}
		}

		// PROCESS SECTION
		m_uiManager->process(delta);
		for (int i = 0; i < m_processableScripts.size(); i++) {
			m_processableScripts[i]->process(delta);
		}

		processScene(m_scene, delta);
		// After matrix updates: draw moving bodies between their last two
		// physics poses so motion is smooth at any frame rate.
		if (m_physicsInterpolation) {
			for (PhysicsWorld* world : m_physicsServer->getWorlds()) {
				world->interpolate(m_physicsClock.getAlpha());
			}
		}
		// PROCESS SECTION END

		m_graphicsPipeline->renderRootScene(m_scene);
		fps++;
		if (secondTime + 1.0 < Window::getGLFWTime()) {
			std::cout << "FPS: " << fps << std::endl;
			fps = 0;
			secondTime = Window::getGLFWTime();
		}
		INPUT->tick();
		m_graphicsPipeline->getWindow()->pollWindow();
		handleResize();
	}

	deinit();
}

void Application::processScene(Scene* scene, float delta) {
	for (auto& obj : scene->getChildren()) {
		if (obj->needsMatrixUpdate()) {
			obj->updateTransformationMatrix();
		}
	}
	for (auto& obj : scene->getChildrenScenes()) {
		processScene(obj, delta);
	}
}

void Application::addProcessableScript(ProcessScriptComponent* script) {
	m_processableScripts.push_back(script);
}
void Application::removeProcessableScript(ProcessScriptComponent* script) {
	m_processableScripts.erase(std::remove(m_processableScripts.begin(),
										   m_processableScripts.end(), script),
							   m_processableScripts.end());
}

void Application::deinit() {
	m_scene->deinit();
	// Scenes own their physics worlds, which must die before the server.
	delete m_scene;
	m_scene = nullptr;
	m_physicsServer.reset();

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

void Application::handleResize() {
	Window* window = m_graphicsPipeline->getWindow().get();
	if (!window->pollResize()) return;
	m_graphicsPipeline->resize(
		window->getFramebufferWidth(), window->getFramebufferHeight(),
		window->getWidth(), window->getHeight());
	m_uiManager->setViewportSize(window->getWidth(), window->getHeight());
}

PhysicsServer* Application::getPhysicsServer() {
	return m_physicsServer.get();
}

FixedStepClock& Application::getPhysicsClock() {
	return m_physicsClock;
}

UIManager* Application::getUIManager() {
	return m_uiManager;
}

}  // namespace bird
//
