#ifndef H_BIRD_PROCESS_SCRIPT_COMPONENT
#define H_BIRD_PROCESS_SCRIPT_COMPONENT

#include <memory>

#include "Script.h"

namespace bird {

class ProcessScriptComponent : public ScriptComponent {
   public:
	void _init() override;
	void _deinit() override;

	virtual void process(float delta);
};

template <typename T>
class ProcessScript : public ProcessScriptComponent {
   public:
	T* object() {
		return (T*)world_object();
	}
};

}  // namespace bird

#endif	// H_BIRD_PROCESS_SCRIPT_COMPONENT
