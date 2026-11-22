/*
 * Browser entry point: starts the engine and hands each frame to the
 * browser's animation loop through emscripten_set_main_loop.
 *
 * The game is linked into the same WebAssembly module (browsers cannot
 * dlopen a native library), so app_entry() is called directly.
 */
#include <emscripten.h>

#include <exception>
#include <string>

#include "../../Application.h"
#include "../../scene/Scene.h"
#include "../../scene/components/Script.h"
#include "../Log.h"

extern "C" bird::Script<bird::Scene>* app_entry();

namespace {

using bird::platform::LogLevel;

void reportFatal(const std::exception& error) {
	bird::platform::log(LogLevel::Error, std::string("Fatal: ") + error.what());
	// clang-format off
	EM_ASM({
		if (typeof Module !== 'undefined' && Module.setStatus) {
			Module.setStatus('Bird Engine stopped: ' + UTF8ToString($0));
		}
	}, error.what());
	// clang-format on
}

void frame(void* application) {
	try {
		static_cast<bird::Application*>(application)->tick();
	} catch (const std::exception& error) {
		reportFatal(error);
		emscripten_cancel_main_loop();
	}
}

}  // namespace

int main() {
	// Lives for the page's lifetime: the main loop keeps running after
	// main() returns.
	auto* application = new bird::Application();
	try {
		application->start(app_entry());
	} catch (const std::exception& error) {
		reportFatal(error);
		return 1;
	}
	// 0 fps: follow requestAnimationFrame (the display's refresh rate).
	emscripten_set_main_loop_arg(frame, application, 0, false);
	return 0;
}
