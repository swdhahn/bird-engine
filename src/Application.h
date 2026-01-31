//
// Created by student on 10/5/23.
//

#ifndef BIRD_APPLICATION_H
#define BIRD_APPLICATION_H

#include <memory>

#include "physics/PhysicsPipeline.h"
#include "renders/GraphicsPipeline.h"
#include "renders/opengl/GLPipeline.h"
#include "util/Input.h"
#include "util/ResourceManager.h"

namespace bird {

class Application {
   public:
	Application(Scene& scene);
	~Application();

	void init();
	void deinit();

	void run();
	void processScene(Scene* scene, float delta);

	void setCamera(Camera* camera);
	Camera* getCamera();

	GraphicsPipeline* getGraphicsPipeline();
	PhysicsPipeline* getPhysicsPipeline();

   private:
	std::unique_ptr<GraphicsPipeline> m_graphicsPipeline;
	std::unique_ptr<PhysicsPipeline> m_physicsPipeline;
	Scene* m_scene;
};

}  // namespace bird

#endif	// BIRD_APPLICATION_H
