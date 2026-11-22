/*
 * NativeActivity entry point: owns the Android event loop and drives the
 * engine through Application::start(), tick() and deinit().
 *
 * The game is a separate shared library in the APK, loaded like the desktop
 * plugin and entered through its exported app_entry(). Its file name comes
 * from the BIRD_ANDROID_GAME_LIBRARY compile definition.
 */
#include <android/log.h>
#include <android_native_app_glue.h>
#include <dlfcn.h>

#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

#include "../../Application.h"
#include "../../scene/Scene.h"
#include "../../scene/components/Script.h"
#include "../Log.h"
#include "../VFS.h"
#include "AndroidApp.h"
#include "AndroidLog.h"
#include "AndroidWindow.h"
#include "ApkAssetVFS.h"

#ifndef BIRD_ANDROID_GAME_LIBRARY
#define BIRD_ANDROID_GAME_LIBRARY "libgame.so"
#endif

namespace {

using bird::platform::AndroidWindow;
using bird::platform::LogLevel;
namespace android = bird::platform::android;

/** What the activity currently allows; rendering needs both. */
struct LauncherState {
	bool hasWindow = false;
	bool resumed = false;
	bool canRender() const { return hasWindow && resumed; }
};

void handleCommand(android_app* app, int32_t command) {
	auto* state = static_cast<LauncherState*>(app->userData);
	switch (command) {
		case APP_CMD_INIT_WINDOW:
			state->hasWindow = app->window != nullptr;
			if (AndroidWindow* window = android::window()) {
				if (state->hasWindow) window->attachSurface(app->window);
			}
			break;
		case APP_CMD_TERM_WINDOW:
			state->hasWindow = false;
			if (AndroidWindow* window = android::window()) {
				window->detachSurface();
			}
			break;
		case APP_CMD_RESUME:
			state->resumed = true;
			break;
		case APP_CMD_PAUSE:
			state->resumed = false;
			break;
		default:
			break;
	}
}

int32_t handleInput(android_app* /*app*/, AInputEvent* event) {
	AndroidWindow* window = android::window();
	return (window != nullptr && window->handleInputEvent(event)) ? 1 : 0;
}

/** Processes queued events; waits for the next one when blocking. */
void pumpEvents(android_app* app, bool block) {
	int events = 0;
	android_poll_source* source = nullptr;
	int timeout = block ? -1 : 0;
	while (ALooper_pollOnce(timeout, nullptr, &events,
							reinterpret_cast<void**>(&source)) >= 0) {
		if (source != nullptr) source->process(app, source);
		if (app->destroyRequested) return;
		timeout = 0;  // after the first event, drain without waiting
	}
}

using AppEntry = bird::Script<bird::Scene>* (*)();

/** @return the game's entry point, or nullptr after logging why. */
AppEntry loadGame() {
	void* handle = dlopen(BIRD_ANDROID_GAME_LIBRARY, RTLD_NOW | RTLD_GLOBAL);
	if (handle == nullptr) {
		bird::platform::log(LogLevel::Error,
							std::string("Cannot load " BIRD_ANDROID_GAME_LIBRARY
										": ") +
								dlerror());
		return nullptr;
	}
	auto entry = reinterpret_cast<AppEntry>(dlsym(handle, "app_entry"));
	if (entry == nullptr) {
		bird::platform::log(LogLevel::Error,
							"The game library exports no app_entry()");
	}
	return entry;
}

}  // namespace

void android_main(android_app* app) {
	android::routeLoggingToLogcat();
	android::setApp(app);
	bird::platform::VFS::set(std::make_unique<bird::platform::ApkAssetVFS>(
		app->activity->assetManager));

	LauncherState state;
	app->userData = &state;
	app->onAppCmd = handleCommand;
	app->onInputEvent = handleInput;

	const AppEntry entry = loadGame();
	if (entry == nullptr) ANativeActivity_finish(app->activity);
	bird::Application application;
	bool started = false;
	while (entry != nullptr && !app->destroyRequested) {
		if (!state.canRender()) {
			pumpEvents(app, true);
			continue;
		}
		try {
			if (!started) {
				application.start(entry());
				started = true;
			}
			// tick() pumps pending events through Window::pollEvents().
			application.tick();
		} catch (const std::exception& error) {
			bird::platform::log(LogLevel::Error,
								std::string("Fatal: ") + error.what());
			ANativeActivity_finish(app->activity);
			break;
		}
	}
	// Wait for Android to finish tearing the activity down.
	while (!app->destroyRequested) pumpEvents(app, true);
	if (started) application.deinit();
	android::setApp(nullptr);
	// Engine globals refer to the destroyed context; a fresh process is the
	// clean way to start the next launch.
	std::exit(0);
}
