//
// Created by student on 10/5/23.
//

#include "Application.h"

#include <cstdio>

#include "animation/AnimationSystem.h"
#include "platform/Clock.h"
#include "platform/Log.h"
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
AnimationSystem* ANIMATION_SYSTEM = nullptr;

Application::Application() {}

Application::~Application() {}

void Application::init(Script<Scene>* script) {
	start(script);
	run();
}

void Application::start(Script<Scene>* script) {
	const double startTime = platform::now();
	auto stage = [startTime](const char* what) {
		char elapsed[32];
		std::snprintf(elapsed, sizeof(elapsed), "%.1f",
					  platform::now() - startTime);
		platform::log(platform::LogLevel::Info,
					  std::string("Startup +") + elapsed + " s: " + what);
	};
	// The pipeline loads its shaders through the cache during init(); the
	// cache's own init() needs the context that the pipeline creates.
	RESOURCE_MANAGER = new ResourceManager();
	m_graphicsPipeline = new gl::GLPipeline();
	m_graphicsPipeline->init();
	RESOURCE_MANAGER->init();
	APPLICATION = this;
	stage("graphics ready");

	INPUT = new Input(m_graphicsPipeline->getWindow().get());
	ANIMATION_SYSTEM = new AnimationSystem();
	m_uiManager = new UIManager();
	m_uiManager->init(RESOURCE_MANAGER->loadFont(UIManager::kDefaultFontPath),
					  m_graphicsPipeline->getWindow().get());
	// The compositor may already have resized the window (e.g. tiling).
	handleResize();
	stage("UI ready");

	// Before the scene: bodies and nav agents are created as soon as objects are added.
	m_physicsServer = _own_mk<PhysicsServer>();
	m_navServer = _own_mk<NavServer>();

	m_scene = new Scene();
	m_scene->addComponent(std::unique_ptr<Script<Scene>>(script));
	stage("loading scene");
	m_scene->init(nullptr);
	stage("scene ready");

	m_lastFrameTime = platform::now();
	m_fpsWindowStart = m_lastFrameTime;
}

void Application::run() {
	while (!m_graphicsPipeline->getWindow()->shouldWindowClose()) {
		tick();
	}

	deinit();
}

void Application::tick() {
	const double frameTime = platform::now();
	const float delta = static_cast<float>(frameTime - m_lastFrameTime);
	m_lastFrameTime = frameTime;

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
	for (size_t i = 0; i < m_processableScripts.size(); i++) {
		m_processableScripts[i]->process(delta);
	}
	// After scripts (animation and navigation win over script-written transforms
	// for the frame), before matrices update and skinning palettes upload.
	m_navServer->step(delta);
	if (m_navDebugDraw) {
		for (NavWorld* world : m_navServer->getWorlds()) {
			world->debugDraw(m_graphicsPipeline->getDebugDraw(), m_navDebugSettings);
		}
	}
	ANIMATION_SYSTEM->tick(delta);

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
	m_framesThisSecond++;
	if (m_fpsWindowStart + 1.0 < platform::now()) {
		platform::log(platform::LogLevel::Info,
					  "FPS: " + std::to_string(m_framesThisSecond));
		m_framesThisSecond = 0;
		m_fpsWindowStart = platform::now();
	}
	INPUT->tick();
	Window* window = m_graphicsPipeline->getWindow().get();
	window->present();
	window->pollEvents();
	handleResize();
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
	// Scenes own their physics and nav worlds, which must die before the servers.
	delete m_scene;
	m_scene = nullptr;
	m_navServer.reset();
	m_physicsServer.reset();
	// After the scene: destroyed objects cancel their own animations.
	delete ANIMATION_SYSTEM;
	ANIMATION_SYSTEM = nullptr;

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

NavServer* Application::getNavServer() {
	return m_navServer.get();
}

FixedStepClock& Application::getPhysicsClock() {
	return m_physicsClock;
}

UIManager* Application::getUIManager() {
	return m_uiManager;
}

}  // namespace bird
//
