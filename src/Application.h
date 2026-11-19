//
// Created by student on 10/5/23.
//

#ifndef BIRD_APPLICATION_H
#define BIRD_APPLICATION_H

#include <algorithm>
#include <vector>

#include "renders/GraphicsPipeline.h"
#include "renders/opengl/GLPipeline.h"
#include "scene/components/ProcessScript.h"
#include "scene/components/Script.h"
#include "util/Input.h"
#include "util/Memory.hpp"
#include "util/ResourceManager.h"

namespace bird {

class UIManager;

class Application {
   public:
	Application();
	~Application();

	void init(Script<Scene>* script);
	void deinit();

	void run();
	void processScene(Scene* scene, float delta);

	void addProcessableScript(ProcessScriptComponent* script);
	void removeProcessableScript(ProcessScriptComponent* script);

	void setCamera(Camera* camera);
	Camera* getCamera();

	GraphicsPipeline* getGraphicsPipeline();
	UIManager* getUIManager();

   private:
	/** Propagates a window size change to the renderer, camera and UI. */
	void handleResize();

	GraphicsPipeline* m_graphicsPipeline;
	UIManager* m_uiManager;
	Scene* m_scene;

	std::vector<ProcessScriptComponent*> m_processableScripts;
};

}  // namespace bird

#endif	// BIRD_APPLICATION_H
