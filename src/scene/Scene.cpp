//
// Created by Seth Hahn on 11/17/22.
//

#include "Scene.h"

#include <memory>

#include "Camera.h"

namespace bird {

Scene::Scene() : WorldObject(), m_childrenScenes(), m_children() {}

void Scene::addChild(_own<WorldObject> worldObject) {
	WorldObject* world = worldObject.get();
	if (world->getType() == SCENE) {
		m_childrenScenes.push_back((Scene*)world);
	}
	m_children.push_back(_mv(worldObject));
	world->init(this);
}

void Scene::removeChild(WorldObject* worldObject) {
	m_children.erase(
		std::remove_if(m_children.begin(), m_children.end(),
					   [worldObject](const _own<WorldObject>& childPtr) {
						   return childPtr.get() == worldObject;
					   }),
		m_children.end());
	if (worldObject->getType() == SCENE) {
		m_childrenScenes.erase(
			std::remove(m_childrenScenes.begin(), m_childrenScenes.end(),
						(Scene*)worldObject),
			m_childrenScenes.end());
	}
	worldObject->deinit();
}

std::vector<_own<WorldObject>>& Scene::getChildren() {
	return m_children;
}

std::vector<Scene*>& Scene::getChildrenScenes() {
	return m_childrenScenes;
}

}  // namespace bird
