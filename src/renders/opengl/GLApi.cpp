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
	if (code != GLEW_OK) {
		throw std::runtime_error(
			std::string("Couldn't initialize GLEW: ") +
			reinterpret_cast<const char*>(glewGetErrorString(code)));
	}
#endif
}

}  // namespace bird::gl
