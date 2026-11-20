#ifndef BIRD_GLFWWINDOW_H
#define BIRD_GLFWWINDOW_H

#include <array>
#include <cstdint>
#include <string>

#include "../../renders/Window.h"

struct GLFWwindow;
struct GLFWcursor;

namespace bird::platform {

/**
 * Desktop window built on GLFW: creates the window and its graphics
 * context, forwards GLFW input callbacks to the WindowEventListener, and
 * serves clipboard, cursors and key names. GLFW key and button codes are
 * the engine's canonical codes, so events pass through unchanged.
 */
class GlfwWindow final : public Window {
   public:
	/**
	 * @param width Initial width in screen points.
	 * @param height Initial height in screen points.
	 * @param title Window caption text.
	 * @throws std::runtime_error when GLFW or the window cannot be created.
	 */
	GlfwWindow(uint32_t width, uint32_t height, const std::string& title);
	~GlfwWindow() override;

	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow& operator=(const GlfwWindow&) = delete;

	bool shouldWindowClose() const override;
	void present() override;
	void pollEvents() override;
	bool pollResize() override;
	void setCursorMode(CursorMode mode) override;
	bool setRawMouseMotion(bool enabled) override;

	std::string getClipboardText() override;
	void setClipboardText(const std::string& text) override;
	void setCursor(UICursor cursor) override;
	std::string keyName(int keyCode) override;

   private:
	static GlfwWindow* fromHandle(GLFWwindow* handle);
	static void onKey(GLFWwindow* handle, int key, int scancode, int action,
					  int mods);
	static void onCharacter(GLFWwindow* handle, unsigned int codepoint);
	static void onMouseButton(GLFWwindow* handle, int button, int action,
							  int mods);
	static void onCursorMove(GLFWwindow* handle, double x, double y);
	static void onScroll(GLFWwindow* handle, double dx, double dy);

	GLFWwindow* m_pWindow = nullptr;
	// Standard cursors, created on first use and owned by the window.
	std::array<GLFWcursor*, 6> m_cursors{};
};

}  // namespace bird::platform

#endif	// BIRD_GLFWWINDOW_H
