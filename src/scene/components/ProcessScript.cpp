#include "ProcessScript.h"

#include "../../Application.h"

namespace bird {

void ProcessScriptComponent::_init() {
	APPLICATION->addProcessableScript(this);
}

void ProcessScriptComponent::_deinit() {
	APPLICATION->removeProcessableScript(this);
}

void ProcessScriptComponent::process(float delta) {}

}  // namespace bird
