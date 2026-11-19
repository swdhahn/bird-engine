#include "ProcessScript.h"

#include "../../Application.h"

namespace bird {

void ProcessScriptComponent::_init() {
	if (APPLICATION) APPLICATION->addProcessableScript(this);
}

void ProcessScriptComponent::_deinit() {
	if (APPLICATION) APPLICATION->removeProcessableScript(this);
}

void ProcessScriptComponent::process(float delta) {}

}  // namespace bird
