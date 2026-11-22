#include "AndroidApp.h"

namespace bird::platform::android {

namespace {
android_app* g_app = nullptr;
AndroidWindow* g_window = nullptr;
}  // namespace

android_app* app() {
	return g_app;
}

void setApp(android_app* app) {
	g_app = app;
}

AndroidWindow* window() {
	return g_window;
}

void setWindow(AndroidWindow* window) {
	g_window = window;
}

}  // namespace bird::platform::android
