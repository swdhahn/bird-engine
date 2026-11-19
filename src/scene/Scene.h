//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_SCENE_H
#define BIRD_SCENE_H

#include <memory>
#include <vector>

#include "WorldObject.h"

namespace bird {

class OptimizationSet {};

class Scene : public WorldObject {
	BIRD_OBJECT(SCENE)
   public:
	Scene();

	void addChild(_own<WorldObject> worldObject);
	void removeChild(WorldObject* worldObject);

	std::vector<_own<WorldObject>>& getChildren();
	std::vector<Scene*>& getChildrenScenes();

   protected:
	Scene* m_parent = nullptr;
	std::vector<_own<WorldObject>> m_children;
	std::vector<Scene*> m_childrenScenes;

   private:
	/**
	 * This will be implemented in the future: But, it will let you define what
	 * specific type of optimization you would want for this scene. i.e. terrain
	 * lod, Oct tree, portals and rooms, etc.
	 */
	OptimizationSet* m_optimizationSet = nullptr;
};

}  // namespace bird

#endif	// BIRD_SCENE_H
