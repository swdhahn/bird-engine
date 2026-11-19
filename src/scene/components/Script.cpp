#include "Script.h"

namespace bird {

void ScriptComponent::init() {}
void ScriptComponent::deinit() {}

Scene* ScriptComponent::scene() {
	return world_object()->getParent();
}
ResourceManager* ScriptComponent::resource_manager() {
	return RESOURCE_MANAGER;
}
Input* ScriptComponent::input() {
	return INPUT;
}

const std::vector<std::unique_ptr<Component>>& ScriptComponent::components() {
	return world_object()->getComponents();
}

void ScriptComponent::addComponent(std::unique_ptr<Component> component) {
	world_object()->addComponent(std::move(component));
}

void ScriptComponent::removeComponent(Component* component) {
	world_object()->removeComponent(component);
}

}  // namespace bird
