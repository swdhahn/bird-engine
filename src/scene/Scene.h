//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_SCENE_H
#define BIRD_SCENE_H

#include <memory>
#include <vector>

#include "Entity.h"
#include "WorldObject.h"

namespace bird {

class OptimizationSet {};

class Scene : public WorldObject {
   public:
	Scene();

	virtual void init() = 0;
	virtual void deinit() = 0;

	/**
	 * An Entity can only be in one Scene at a time!
	 */
	void addEntity(std::unique_ptr<Entity> entity);
	std::unique_ptr<Entity> removeEntity(Entity* entity);

	/**
	 * A Scene can only be the child of one Scene at a time!
	 */

	void addChild(std::unique_ptr<Scene> scene);
	std::unique_ptr<Scene> removeChild(Scene* scene);

	const std::vector<std::unique_ptr<Scene>>& getChildren() const;
	const std::vector<std::unique_ptr<Entity>>& getEntities() const;

   protected:
	Scene* m_parent = nullptr;
	std::vector<std::unique_ptr<Scene>> m_children;
	std::vector<std::unique_ptr<Entity>> m_entities;

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
