//
// Created by student on 10/5/23.
//

#ifndef BIRD_APPLICATION_H
#define BIRD_APPLICATION_H

#include <algorithm>
#include <vector>

#include "physics/FixedStepClock.h"
#include "physics/PhysicsServer.h"
#include "physics/PhysicsWorld.h"
#include "renders/GraphicsPipeline.h"
#include "renders/opengl/GLPipeline.h"
#include "scene/components/ProcessScript.h"
#include "scene/components/Script.h"
#include "util/Input.h"
#include "util/Memory.hpp"
#include "util/ResourceManager.h"

namespace bird {

class UIManager;

/**
 * Central engine orchestrator.
 *
 * Manages the lifecycle of core engine systems: graphics pipeline, window
 * event handling, input system, asset/resource manager, 2D UI manager,
 * and the active scene hierarchy. Runs the main frame loop and coordinates
 * per-frame script ticking and scene rendering.
 */
class Application {
   public:
	Application();
	~Application();

	/**
	 * Initializes engine systems and enters the main loop.
	 *
	 * Instantiates the default graphics pipeline (OpenGL), initializes windowing,
	 * input callbacks, resource cache, and 2D UI manager. Creates the root Scene,
	 * attaches the provided entry script, and begins the frame loop in run().
	 *
	 * @param script User application entry script; ownership is transferred
	 *               to the root Scene.
	 */
	void init(Script<Scene>* script);

	/**
	 * Tears down engine systems and releases allocated resources.
	 */
	void deinit();

	/**
	 * Executes the continuous engine frame loop.
	 *
	 * Each frame:
	 * 1. Computes delta time from the window clock.
	 * 2. Updates camera matrices if dirty.
	 * 3. Ticks UIManager layout, animations, and modals.
	 * 4. Calls process(delta) on all registered ProcessScript instances.
	 * 5. Recursively updates dirty scene transform matrices via processScene().
	 * 6. Renders the active scene and overlays UI via the graphics pipeline.
	 * 7. Ticks the input manager and polls platform window events.
	 */
	void run();

	/**
	 * Recursively traverses a scene hierarchy to resolve dirty transforms.
	 *
	 * Any WorldObject with a dirty transform (needsMatrixUpdate() == true)
	 * has its world-space transformation matrix recalculated before rendering.
	 *
	 * @param scene The root or sub-scene to process.
	 * @param delta Elapsed time since the previous frame in seconds.
	 */
	void processScene(Scene* scene, float delta);

	/**
	 * Registers a script to receive per-frame process(delta) callbacks.
	 * Called automatically by ProcessScriptComponent::_init().
	 */
	void addProcessableScript(ProcessScriptComponent* script);

	/**
	 * Unregisters a script from per-frame updates.
	 * Called automatically by ProcessScriptComponent::_deinit().
	 */
	void removeProcessableScript(ProcessScriptComponent* script);

	void setCamera(Camera* camera);
	Camera* getCamera();

	GraphicsPipeline* getGraphicsPipeline();
	UIManager* getUIManager();

	/** Global physics state; every scene's PhysicsWorld is stepped through it. */
	PhysicsServer* getPhysicsServer();
	/** The fixed-step clock driving physicsProcess() and physics steps. */
	FixedStepClock& getPhysicsClock();

	/**
	 * Draws every physics world's colliders as wireframes each frame (into
	 * GraphicsPipeline::getDebugDraw()), configured by getPhysicsDebugSettings().
	 */
	void setPhysicsDebugDraw(bool enabled) { m_physicsDebugDraw = enabled; }
	bool isPhysicsDebugDrawEnabled() const { return m_physicsDebugDraw; }
	PhysicsDebugSettings& getPhysicsDebugSettings() { return m_physicsDebugSettings; }

	/** Smooth rendering between physics steps (on by default). */
	void setPhysicsInterpolation(bool enabled) { m_physicsInterpolation = enabled; }
	bool isPhysicsInterpolationEnabled() const { return m_physicsInterpolation; }

   private:
	/** Propagates a window size change to the renderer, camera and UI. */
	void handleResize();

	GraphicsPipeline* m_graphicsPipeline;
	UIManager* m_uiManager;
	_own<PhysicsServer> m_physicsServer;
	FixedStepClock m_physicsClock;
	bool m_physicsDebugDraw = false;
	bool m_physicsInterpolation = true;
	PhysicsDebugSettings m_physicsDebugSettings;
	Scene* m_scene;

	std::vector<ProcessScriptComponent*> m_processableScripts;
};

}  // namespace bird

#endif	// BIRD_APPLICATION_H
