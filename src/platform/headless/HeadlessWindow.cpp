#include "HeadlessWindow.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace bird::platform {

namespace {

constexpr uint64_t kDefaultFrameLimit = 120;

uint64_t frameLimitFromEnvironment() {
	const char* value = std::getenv("BIRD_HEADLESS_FRAMES");
	if (value == nullptr) return kDefaultFrameLimit;
	char* end = nullptr;
	const unsigned long long frames = std::strtoull(value, &end, 10);
	if (end == value || *end != '\0' || frames == 0) {
		throw std::invalid_argument(
			"BIRD_HEADLESS_FRAMES must be a positive integer");
	}
	return frames;
}

[[noreturn]] void failEgl(const std::string& what) {
	char code[16];
	std::snprintf(code, sizeof(code), "0x%04X",
				  static_cast<unsigned>(eglGetError()));
	throw std::runtime_error("HeadlessWindow: " + what +
							 " failed (EGL error " + code + ")");
}

/** Prefers Mesa's surfaceless platform, which needs no display server. */
EGLDisplay openDisplay() {
	const char* extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (extensions != nullptr &&
		std::strstr(extensions, "EGL_MESA_platform_surfaceless") != nullptr) {
		auto getPlatformDisplay =
			reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
				eglGetProcAddress("eglGetPlatformDisplayEXT"));
		if (getPlatformDisplay != nullptr) {
			EGLDisplay display = getPlatformDisplay(
				EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
			if (display != EGL_NO_DISPLAY) return display;
		}
	}
	return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

}  // namespace

HeadlessWindow::HeadlessWindow(uint32_t width, uint32_t height)
	: m_surfaceWidth(width),
	  m_surfaceHeight(height),
	  m_frameLimit(frameLimitFromEnvironment()) {
#if defined(BIRD_GL_ES)
	const EGLenum api = EGL_OPENGL_ES_API;
	const EGLint renderable = EGL_OPENGL_ES3_BIT;
	const EGLint contextAttributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3,
										EGL_CONTEXT_MINOR_VERSION, 0,
										EGL_NONE};
#else
	const EGLenum api = EGL_OPENGL_API;
	const EGLint renderable = EGL_OPENGL_BIT;
	const EGLint contextAttributes[] = {
		EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 1,
		EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
		EGL_NONE};
#endif
	EGLDisplay display = openDisplay();
	if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr)) {
		failEgl("eglInitialize");
	}
	m_display = display;

	const EGLint configAttributes[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, renderable,
		EGL_RED_SIZE,	  8,			   EGL_GREEN_SIZE,		8,
		EGL_BLUE_SIZE,	  8,			   EGL_ALPHA_SIZE,		8,
		EGL_DEPTH_SIZE,	  24,			   EGL_STENCIL_SIZE,	8,
		EGL_NONE};
	EGLConfig config = nullptr;
	EGLint configCount = 0;
	if (!eglChooseConfig(display, configAttributes, &config, 1,
						 &configCount) ||
		configCount == 0) {
		failEgl("eglChooseConfig");
	}
	if (!eglBindAPI(api)) failEgl("eglBindAPI");

	const EGLint surfaceAttributes[] = {
		EGL_WIDTH, static_cast<EGLint>(width), EGL_HEIGHT,
		static_cast<EGLint>(height), EGL_NONE};
	EGLSurface surface =
		eglCreatePbufferSurface(display, config, surfaceAttributes);
	if (surface == EGL_NO_SURFACE) failEgl("eglCreatePbufferSurface");
	m_surface = surface;

	EGLContext context =
		eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
	if (context == EGL_NO_CONTEXT) failEgl("eglCreateContext");
	m_context = context;

	if (!eglMakeCurrent(display, surface, surface, context)) {
		failEgl("eglMakeCurrent");
	}
}

HeadlessWindow::~HeadlessWindow() {
	EGLDisplay display = static_cast<EGLDisplay>(m_display);
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(display, static_cast<EGLContext>(m_context));
	eglDestroySurface(display, static_cast<EGLSurface>(m_surface));
	eglTerminate(display);
}

bool HeadlessWindow::shouldWindowClose() const {
	return m_framesPresented >= m_frameLimit;
}

void HeadlessWindow::present() {
	eglSwapBuffers(static_cast<EGLDisplay>(m_display),
				   static_cast<EGLSurface>(m_surface));
	m_framesPresented++;
}

bool HeadlessWindow::pollResize() {
	const int width = static_cast<int>(m_surfaceWidth);
	const int height = static_cast<int>(m_surfaceHeight);
	return updateSizes(width, height, width, height);
}

}  // namespace bird::platform
