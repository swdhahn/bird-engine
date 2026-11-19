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

/**
 * Container and spatial grouping node for entities in the scene tree.
 *
 * Inherits from WorldObject and owns child WorldObject instances via unique
 * pointers (_own<WorldObject>). Can contain nested Scene subtrees. The
 * Application recursively traverses scenes via Application::processScene()
 * to resolve transform matrices and render geometry.
 */
class Scene : public WorldObject {
	BIRD_OBJECT(SCENE)
   public:
	Scene();

	/**
	 * Takes ownership of a child entity and associates it with this Scene.
	 *
	 * Initializes the child with this scene as parent. If the child is also
	 * a Scene (getType() == SCENE), it is also tracked in getChildrenScenes().
	 *
	 * @param worldObject Unique pointer to the entity being added.
	 */
	void addChild(_own<WorldObject> worldObject);

	/**
	 * Deinitializes and removes a child entity from this scene.
	 *
	 * @param worldObject Raw pointer to the child to be destroyed.
	 */
	void removeChild(WorldObject* worldObject);

	std::vector<_own<WorldObject>>& getChildren();
	std::vector<Scene*>& getChildrenScenes();

   protected:
	Scene* m_parent = nullptr;
	std::vector<_own<WorldObject>> m_children;
	std::vector<Scene*> m_childrenScenes;

   private:
	/**
	 * Spatial acceleration or optimization set (octree, BVH, LOD, portal rooms).
	 */
	OptimizationSet* m_optimizationSet = nullptr;
};

}  // namespace bird

#endif	// BIRD_SCENE_H
