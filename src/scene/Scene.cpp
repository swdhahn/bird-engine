//
// Created by Seth Hahn on 11/17/22.
//

#include "Scene.h"

#include <memory>

#include "Entity.h"

namespace bird {

Scene::Scene() {}

void Scene::addEntity(std::unique_ptr<Entity> entity) {
	entity->setScene(this);
	Entity* e = entity.get();
	m_entities.push_back(std::move(entity));
	e->init();
}

std::unique_ptr<Entity> Scene::removeEntity(Entity* entity) {
	auto ne = std::find_if(m_entities.begin(), m_entities.end(),
			[entity](const std::unique_ptr<Entity>& s) { return s->getID() == entity->getID(); });
	std::unique_ptr<Entity> e = std::move(*ne);
	m_entities.erase(ne, m_entities.end());
	entity->_deinit();

	return e;
}

void Scene::addChild(std::unique_ptr<Scene> scene) {
	Scene* s = scene.get();
	m_children.push_back(std::move(scene));
	s->init();
}

std::unique_ptr<Scene> Scene::removeChild(Scene* scene) {
	auto ne = std::find_if(m_children.begin(), m_children.end(),
			[scene](const std::unique_ptr<Scene>& s) { return s->getID() == scene->getID(); });
	std::unique_ptr<Scene> s = std::move(*ne);
	m_children.erase(ne, m_children.end());

	scene->deinit();
	return s;
}

const std::vector<std::unique_ptr<Scene>>& Scene::getChildren() const {
	return m_children;
}

const std::vector<std::unique_ptr<Entity>>& Scene::getEntities() const {
	return m_entities;
}

}  // namespace bird
