//
// Created by Seth Hahn on 11/30/22.
//

#include "Entity.h"

namespace bird {

Entity::Entity() : m_scene(nullptr) {}

void Entity::setScene(Scene* scene) {
	if (m_scene != nullptr) {
		std::cerr << "Entity is already in a scene!" << std::endl;
		std::exit(-1);
	}
	m_scene = scene;
}

Scene* Entity::getScene() {
	return m_scene;
}

}  // namespace bird
