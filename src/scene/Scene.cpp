//
// Created by Seth Hahn on 11/17/22.
//

#include "Scene.h"

#include <memory>
#include <stdexcept>

#include "../navigation/NavServer.h"
#include "../physics/PhysicsServer.h"
#include "Camera.h"

namespace bird {

Scene::Scene() : WorldObject(), m_childrenScenes(), m_children() {}

Scene::~Scene() = default;

void Scene::deinit() {
	for (auto& child : m_children) {
		child->deinit();
	}
	WorldObject::deinit();
}

void Scene::addChild(_own<WorldObject> worldObject) {
	WorldObject* world = worldObject.get();
	if (world->getType() == SCENE) {
		m_childrenScenes.push_back((Scene*)world);
	}
	m_children.push_back(_mv(worldObject));
	world->init(this);
}

_own<WorldObject> Scene::detachChild(WorldObject* worldObject) {
	auto it = std::find_if(m_children.begin(), m_children.end(),
						   [worldObject](const _own<WorldObject>& childPtr) {
							   return childPtr.get() == worldObject;
						   });
	if (it == m_children.end()) {
		throw std::invalid_argument("Scene::detachChild: not a child of this scene");
	}
	// Deinitialize while still attached, so components can reach the scene.
	worldObject->deinit();
	_own<WorldObject> owned = _mv(*it);
	m_children.erase(it);
	if (worldObject->getType() == SCENE) {
		m_childrenScenes.erase(
			std::remove(m_childrenScenes.begin(), m_childrenScenes.end(),
						(Scene*)worldObject),
			m_childrenScenes.end());
	}
	return owned;
}

void Scene::removeChild(WorldObject* worldObject) {
	detachChild(worldObject);
}

std::vector<_own<WorldObject>>& Scene::getChildren() {
	return m_children;
}

std::vector<Scene*>& Scene::getChildrenScenes() {
	return m_childrenScenes;
}

PhysicsWorld* Scene::createPhysicsWorld(const PhysicsWorldSettings& settings) {
	if (m_physicsWorld) {
		throw std::logic_error("Scene::createPhysicsWorld: scene already owns a world");
	}
	PhysicsServer* server = PhysicsServer::get();
	if (server == nullptr) {
		throw std::logic_error("Scene::createPhysicsWorld: no PhysicsServer exists");
	}
	m_physicsWorld = _own_mk<PhysicsWorld>(*server, settings);
	return m_physicsWorld.get();
}

PhysicsWorld* Scene::getPhysicsWorld() {
	for (Scene* scene = this; scene != nullptr; scene = scene->getParent()) {
		if (scene->m_physicsWorld) return scene->m_physicsWorld.get();
		if (scene->getParent() == nullptr) return scene->createPhysicsWorld();
	}
	return nullptr;	 // unreachable: the loop always reaches the root
}

bool Scene::ownsPhysicsWorld() const {
	return m_physicsWorld != nullptr;
}

NavWorld* Scene::createNavWorld(const NavWorldSettings& settings) {
	if (m_navWorld) {
		throw std::logic_error("Scene::createNavWorld: scene already owns a NavWorld");
	}
	NavServer* server = NavServer::get();
	if (server == nullptr) {
		throw std::logic_error("Scene::createNavWorld: no NavServer exists");
	}
	m_navWorld = _own_mk<NavWorld>(*server, settings);
	return m_navWorld.get();
}

NavWorld* Scene::getNavWorld() {
	for (Scene* scene = this; scene != nullptr; scene = scene->getParent()) {
		if (scene->m_navWorld) return scene->m_navWorld.get();
		if (scene->getParent() == nullptr) return scene->createNavWorld();
	}
	return nullptr;	 // unreachable: the loop always reaches the root
}

bool Scene::ownsNavWorld() const {
	return m_navWorld != nullptr;
}

}  // namespace bird
