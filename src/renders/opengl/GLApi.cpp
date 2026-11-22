#include "GLApi.h"

#include <stdexcept>
#include <string>

namespace bird::gl {

void loadGLFunctions() {
#if defined(BIRD_GL_ES)
	// libGLESv3 (Android), the OpenGLES framework (iOS) and Emscripten's
	// WebGL bindings export every ES 3.0 entry point; nothing to resolve.
#else
	glewExperimental = GL_TRUE;
	const GLenum code = glewInit();
#if defined(GLEW_ERROR_NO_GLX_DISPLAY)
	// GL entry points are loaded before GLEW looks for GLX; contexts made
	// through EGL (Wayland, headless) have no GLX display, which only
	// affects GLX extensions the engine never uses.
	if (code == GLEW_ERROR_NO_GLX_DISPLAY) return;
#endif
	if (code != GLEW_OK) {
		throw std::runtime_error(
			std::string("Couldn't initialize GLEW: ") +
			reinterpret_cast<const char*>(glewGetErrorString(code)));
	}
#endif
}

}  // namespace bird::gl
