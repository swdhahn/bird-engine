//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_SCENE_H
#define BIRD_SCENE_H

#include <memory>
#include <vector>

#include "../physics/PhysicsWorld.h"
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
	~Scene() override;

	/** Deinitializes every child (recursively for sub-scenes), then itself. */
	void deinit() override;

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

	/**
	 * Deinitializes a child and hands its ownership back, so the object can
	 * be added to another scene (its physics body is rebuilt there).
	 *
	 * @param worldObject Raw pointer to a direct child of this scene.
	 * @return The detached object.
	 * @throws std::invalid_argument when worldObject is not a direct child.
	 */
	_own<WorldObject> detachChild(WorldObject* worldObject);

	std::vector<_own<WorldObject>>& getChildren();
	std::vector<Scene*>& getChildrenScenes();

	/**
	 * Gives this scene its own isolated physics simulation. Bodies in this
	 * scene and its sub-scenes (down to the next scene owning a world) live
	 * there and never interact with bodies of other worlds. Call before
	 * adding bodies.
	 *
	 * @throws std::logic_error when this scene already owns a world or no
	 *         PhysicsServer exists.
	 */
	PhysicsWorld* createPhysicsWorld(const PhysicsWorldSettings& settings = {});

	/**
	 * The world bodies in this scene belong to: this scene's own world, else
	 * the nearest ancestor's. The root scene creates a default world lazily,
	 * so ordinary games never call createPhysicsWorld().
	 *
	 * @throws std::logic_error when a world must be created and no
	 *         PhysicsServer exists.
	 */
	PhysicsWorld* getPhysicsWorld();

	/** True when this scene owns a world (rather than using an ancestor's). */
	bool ownsPhysicsWorld() const;

   protected:
	// Declared before m_children so children (and their bodies) are
	// destroyed while the world is still alive.
	_own<PhysicsWorld> m_physicsWorld;
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
