#include "AndroidWindow.h"

#include <EGL/eglext.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <cstdio>
#include <stdexcept>
#include <string>

#include "../../util/Input.h"
#include "../Platform.h"
#include "AndroidApp.h"
#include "AndroidKeyMap.h"

namespace bird::platform {

namespace {

[[noreturn]] void failEgl(const std::string& what) {
	char code[16];
	std::snprintf(code, sizeof(code), "0x%04X",
				  static_cast<unsigned>(eglGetError()));
	throw std::runtime_error("AndroidWindow: " + what +
							 " failed (EGL error " + code + ")");
}

/** Pixels per density-independent pixel for the app's configuration. */
float densityScale(android_app* app) {
	if (app == nullptr || app->config == nullptr) return 1.0f;
	const int32_t density = AConfiguration_getDensity(app->config);
	if (density <= 0 || density == ACONFIGURATION_DENSITY_ANY ||
		density == ACONFIGURATION_DENSITY_NONE) {
		return 1.0f;
	}
	return static_cast<float>(density) / ACONFIGURATION_DENSITY_MEDIUM;
}

/** Picks an RGBA8 ES 3.0 window config, preferring depth 24 + stencil 8. */
EGLConfig chooseConfig(EGLDisplay display) {
	const EGLint depthStencil[][2] = {{24, 8}, {16, 0}, {0, 0}};
	for (const auto& [depth, stencil] : depthStencil) {
		const EGLint attributes[] = {
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
			EGL_SURFACE_TYPE,	 EGL_WINDOW_BIT,
			EGL_RED_SIZE,		 8,
			EGL_GREEN_SIZE,		 8,
			EGL_BLUE_SIZE,		 8,
			EGL_ALPHA_SIZE,		 8,
			EGL_DEPTH_SIZE,		 depth,
			EGL_STENCIL_SIZE,	 stencil,
			EGL_NONE};
		EGLConfig config = nullptr;
		EGLint count = 0;
		if (eglChooseConfig(display, attributes, &config, 1, &count) &&
			count > 0) {
			return config;
		}
	}
	failEgl("eglChooseConfig");
}

}  // namespace

std::unique_ptr<Window> createWindow(uint32_t /*width*/, uint32_t /*height*/,
									 const std::string& /*title*/) {
	android_app* app = android::app();
	if (app == nullptr || app->window == nullptr) {
		throw std::runtime_error(
			"createWindow: the activity has no window yet");
	}
	return std::make_unique<AndroidWindow>(app->window);
}

AndroidWindow::AndroidWindow(ANativeWindow* nativeWindow) {
	m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (m_display == EGL_NO_DISPLAY ||
		!eglInitialize(m_display, nullptr, nullptr)) {
		failEgl("eglInitialize");
	}
	m_config = chooseConfig(m_display);
	const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
										EGL_NONE};
	m_context = eglCreateContext(m_display, m_config, EGL_NO_CONTEXT,
								 contextAttributes);
	if (m_context == EGL_NO_CONTEXT) failEgl("eglCreateContext");
	m_scale = densityScale(android::app());
	createSurface(nativeWindow);
	android::setWindow(this);
}

AndroidWindow::~AndroidWindow() {
	android::setWindow(nullptr);
	detachSurface();
	eglDestroyContext(m_display, m_context);
	eglTerminate(m_display);
}

void AndroidWindow::createSurface(ANativeWindow* nativeWindow) {
	// The window's buffers must match the config's native pixel format.
	EGLint format = 0;
	eglGetConfigAttrib(m_display, m_config, EGL_NATIVE_VISUAL_ID, &format);
	ANativeWindow_setBuffersGeometry(nativeWindow, 0, 0, format);
	m_surface =
		eglCreateWindowSurface(m_display, m_config, nativeWindow, nullptr);
	if (m_surface == EGL_NO_SURFACE) failEgl("eglCreateWindowSurface");
	if (!eglMakeCurrent(m_display, m_surface, m_surface, m_context)) {
		failEgl("eglMakeCurrent");
	}
	eglSwapInterval(m_display, 1);	// pace frames to the display
}

void AndroidWindow::attachSurface(ANativeWindow* nativeWindow) {
	detachSurface();
	createSurface(nativeWindow);
}

void AndroidWindow::detachSurface() {
	if (m_surface == EGL_NO_SURFACE) return;
	// Keep the context current without a surface so GPU objects survive.
	eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
	eglDestroySurface(m_display, m_surface);
	m_surface = EGL_NO_SURFACE;
}

bool AndroidWindow::shouldWindowClose() const {
	android_app* app = android::app();
	return app == nullptr || app->destroyRequested != 0;
}

void AndroidWindow::present() {
	if (m_surface == EGL_NO_SURFACE) return;
	if (!eglSwapBuffers(m_display, m_surface)) {
		const EGLint error = eglGetError();
		if (error == EGL_CONTEXT_LOST) {
			throw std::runtime_error(
				"AndroidWindow: the OpenGL ES context was lost");
		}
		// EGL_BAD_SURFACE and friends: the window is going away; the
		// launcher reattaches when Android hands out a new one.
	}
}

void AndroidWindow::pollEvents() {
	android_app* app = android::app();
	if (app == nullptr) return;
	int events = 0;
	android_poll_source* source = nullptr;
	// Non-blocking: drain whatever commands and input are queued.
	while (ALooper_pollOnce(0, nullptr, &events,
							reinterpret_cast<void**>(&source)) >= 0) {
		if (source != nullptr) source->process(app, source);
		if (app->destroyRequested) break;
	}
}

bool AndroidWindow::pollResize() {
	if (m_surface == EGL_NO_SURFACE) return false;
	EGLint width = 0, height = 0;
	eglQuerySurface(m_display, m_surface, EGL_WIDTH, &width);
	eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &height);
	return updateSizes(width, height,
					   static_cast<int>(static_cast<float>(width) / m_scale),
					   static_cast<int>(static_cast<float>(height) / m_scale));
}

bool AndroidWindow::handleInputEvent(const AInputEvent* event) {
	switch (AInputEvent_getType(event)) {
		case AINPUT_EVENT_TYPE_MOTION:
			return handleMotion(event);
		case AINPUT_EVENT_TYPE_KEY:
			return handleKey(event);
		default:
			return false;
	}
}

bool AndroidWindow::handleMotion(const AInputEvent* event) {
	WindowEventListener* target = listener();
	if (target == nullptr) return false;
	const int32_t action = AMotionEvent_getAction(event);
	const int32_t kind = action & AMOTION_EVENT_ACTION_MASK;
	const size_t index = static_cast<size_t>(
		(action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
		AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
	const int32_t pointer = AMotionEvent_getPointerId(event, index);
	auto x = [&](size_t i) {
		return static_cast<double>(AMotionEvent_getX(event, i) / m_scale);
	};
	auto y = [&](size_t i) {
		return static_cast<double>(AMotionEvent_getY(event, i) / m_scale);
	};

	// The first finger down acts as the left mouse button until it lifts.
	switch (kind) {
		case AMOTION_EVENT_ACTION_DOWN:
			m_primaryPointer = pointer;
			target->onCursorMove(x(index), y(index));
			target->onMouseButton(MOUSE_BUTTON_LEFT, InputAction::Press, 0);
			return true;
		case AMOTION_EVENT_ACTION_MOVE: {
			const size_t count = AMotionEvent_getPointerCount(event);
			for (size_t i = 0; i < count; i++) {
				if (AMotionEvent_getPointerId(event, i) == m_primaryPointer) {
					target->onCursorMove(x(i), y(i));
				}
			}
			return true;
		}
		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP:
		case AMOTION_EVENT_ACTION_CANCEL:
			if (pointer == m_primaryPointer ||
				kind == AMOTION_EVENT_ACTION_CANCEL) {
				target->onCursorMove(x(index), y(index));
				target->onMouseButton(MOUSE_BUTTON_LEFT, InputAction::Release,
									  0);
				m_primaryPointer = -1;
			}
			return true;
		case AMOTION_EVENT_ACTION_HOVER_MOVE:
			target->onCursorMove(x(index), y(index));
			return true;
		case AMOTION_EVENT_ACTION_SCROLL:
			target->onScroll(
				AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL,
										  index),
				AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL,
										  index));
			return true;
		default:
			return false;
	}
}

bool AndroidWindow::handleKey(const AInputEvent* event) {
	const int32_t code = AKeyEvent_getKeyCode(event);
	if (code == AKEYCODE_BACK) return false;  // let Android navigate back
	WindowEventListener* target = listener();
	const int key = engineKeyFromAndroid(code);
	if (target == nullptr || key == KEY_UNKNOWN) return false;
	const int32_t action = AKeyEvent_getAction(event);
	InputAction mapped = InputAction::Release;
	if (action == AKEY_EVENT_ACTION_DOWN) {
		mapped = AKeyEvent_getRepeatCount(event) > 0 ? InputAction::Repeat
													 : InputAction::Press;
	} else if (action != AKEY_EVENT_ACTION_UP) {
		return false;
	}
	target->onKey(key, mapped,
				  engineModifiersFromAndroid(AKeyEvent_getMetaState(event)));
	return true;
}

}  // namespace bird::platform
