//
// Created by Seth Hahn on 11/17/22.
//

#include "Scene.h"

namespace bird {

Scene::Scene() {}

void Scene::addEntity(Entity* entity) {
	m_entities.push_back(entity);
	entity->init();
}

void Scene::removeEntity(Entity* entity) {
	m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity),
					 m_entities.end());
	entity->_deinit();
}

void Scene::addChild(Scene* scene) {
	m_children.push_back(scene);
	scene->init();
}

void Scene::removeChild(Scene* scene) {
	m_children.erase(std::remove(m_children.begin(), m_children.end(), scene),
					 m_children.end());
	scene->deinit();
}

std::vector<Scene*> Scene::getChildren() const {
	return m_children;
}

std::vector<Entity*> Scene::getEntities() const {
	return m_entities;
}

}  // namespace bird
