#ifndef BIRD_GLAPI_H
#define BIRD_GLAPI_H

/*
 * The only header in the engine that includes OpenGL declarations. Every
 * file of the GL backend includes this one instead of a platform header.
 *
 * The profile is chosen at build time (CMake option BIRD_GL_PROFILE):
 *   DESKTOP - OpenGL 4.1+ core, functions loaded through GLEW.
 *   ES      - OpenGL ES 3.0+, functions linked directly. Defines BIRD_GL_ES.
 *
 * The ES profile serves every embedded target through one code path:
 *   Android            <GLES3/gl3.h> from the NDK
 *   iOS (deprecated)   <OpenGLES/ES3/gl.h> from the OpenGLES framework
 *   WebGL 2            <GLES3/gl3.h> from Emscripten; defines BIRD_GL_WEBGL
 *   Linux (testing)    <GLES3/gl3.h> from Mesa
 *
 * Features beyond the profile's baseline are detected at run time through
 * GLCapabilities, never through #ifdef.
 */

#if defined(BIRD_GL_ES)
#if defined(__APPLE__)
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#endif
#if defined(__EMSCRIPTEN__) && !defined(BIRD_GL_WEBGL)
#define BIRD_GL_WEBGL
#endif
#else
#include <GL/glew.h>
#endif

/*
 * Tokens that ES 3.0 headers lack. Each shares its value with the desktop
 * token and with the ES extension or ES 3.2 token that exposes it, so code
 * may pass them wherever GLCapabilities reports the feature present.
 */
#ifndef GL_DEPTH_CLAMP
#define GL_DEPTH_CLAMP 0x864F  // EXT_depth_clamp
#endif
#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB 0x8DB9	// EXT_sRGB_write_control
#endif
#ifndef GL_DEBUG_OUTPUT
#define GL_DEBUG_OUTPUT 0x92E0	// KHR_debug, ES 3.2
#endif
#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER 0x8DD9  // EXT_geometry_shader, ES 3.2
#endif
#ifndef GL_TESS_CONTROL_SHADER
#define GL_TESS_CONTROL_SHADER 0x8E88  // EXT_tessellation_shader, ES 3.2
#endif
#ifndef GL_TESS_EVALUATION_SHADER
#define GL_TESS_EVALUATION_SHADER 0x8E87  // EXT_tessellation_shader, ES 3.2
#endif

namespace bird::gl {

/**
 * Resolves GL entry points for the current context. Desktop: initializes
 * GLEW. ES: nothing to resolve, since the platform library exports the
 * functions directly.
 *
 * @throws std::runtime_error when the loader fails.
 */
void loadGLFunctions();

}  // namespace bird::gl

#endif	// BIRD_GLAPI_H
