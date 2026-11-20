#ifndef BIRD_ANDROIDAPP_H
#define BIRD_ANDROIDAPP_H

struct android_app;

namespace bird::platform {

class AndroidWindow;

/**
 * Process-wide handles of the Android backend. The launcher (AndroidMain)
 * sets the app before the engine starts; AndroidWindow registers itself
 * so the launcher can route surface changes and input to it.
 */
namespace android {

/** @return the NativeActivity glue state; nullptr before android_main. */
android_app* app();
void setApp(android_app* app);

/** @return the live window, or nullptr before the engine created it. */
AndroidWindow* window();
void setWindow(AndroidWindow* window);

}  // namespace android

}  // namespace bird::platform

#endif	// BIRD_ANDROIDAPP_H
