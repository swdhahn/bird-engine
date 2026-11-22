#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "platform/Clock.h"
#include "platform/Log.h"
#include "platform/VFS.h"
#include "renders/Window.h"
#include "util/Input.h"

using bird::CursorMode;
using bird::Input;
using bird::InputAction;

namespace {

/** Records what Input asks of the platform; sizes are set by tests. */
class FakeWindow final : public bird::Window {
   public:
	bool shouldWindowClose() const override { return false; }
	void present() override {}
	void pollEvents() override {}
	bool pollResize() override {
		return updateSizes(framebufferWidth, framebufferHeight, width, height);
	}
	void setCursorMode(CursorMode mode) override { cursorMode = mode; }
	bool setRawMouseMotion(bool enabled) override {
		rawMotion = enabled;
		return true;
	}
	std::string getClipboardText() override { return {}; }
	void setClipboardText(const std::string&) override {}
	void setCursor(bird::UICursor) override {}

	bird::WindowEventListener* currentListener() const { return listener(); }

	int framebufferWidth = 0, framebufferHeight = 0, width = 0, height = 0;
	CursorMode cursorMode = CursorMode::Normal;
	bool rawMotion = false;
};

}  // namespace

TEST(Window, FirstPollReportsAndUnchangedSizeDoesNot) {
	FakeWindow window;
	window.framebufferWidth = 2560;
	window.framebufferHeight = 1440;
	window.width = 1280;
	window.height = 720;
	EXPECT_TRUE(window.pollResize());
	EXPECT_EQ(window.getFramebufferWidth(), 2560u);
	EXPECT_EQ(window.getHeight(), 720u);
	EXPECT_FALSE(window.pollResize());
	window.width = 1300;
	EXPECT_TRUE(window.pollResize());
}

TEST(Window, MinimizedWindowKeepsLastUsableSize) {
	FakeWindow window;
	window.framebufferWidth = window.width = 800;
	window.framebufferHeight = window.height = 600;
	ASSERT_TRUE(window.pollResize());
	window.framebufferWidth = window.width = 0;
	EXPECT_FALSE(window.pollResize());
	EXPECT_EQ(window.getWidth(), 800u);
}

TEST(Input, SubscribesToItsWindowForItsLifetime) {
	FakeWindow window;
	{
		Input input(&window);
		EXPECT_EQ(window.currentListener(), &input);
	}
	EXPECT_EQ(window.currentListener(), nullptr);
}

TEST(Input, ActionsFollowKeyAndButtonEvents) {
	FakeWindow window;
	Input input(&window);
	input.addAction(bird::Action("jump").addKey(KEY_SPACE));
	input.addAction(bird::Action("fire").addMouseButton(MOUSE_BUTTON_LEFT));

	input.onKey(KEY_SPACE, InputAction::Press, 0);
	input.tick();
	EXPECT_EQ(input.getActionValue("jump"), 1.0f);
	EXPECT_EQ(input.getActionValue("fire"), 0.0f);

	// Repeats keep the key held; release clears it.
	input.onKey(KEY_SPACE, InputAction::Repeat, 0);
	input.tick();
	EXPECT_EQ(input.getActionValue("jump"), 1.0f);
	input.onKey(KEY_SPACE, InputAction::Release, 0);
	input.onMouseButton(MOUSE_BUTTON_LEFT, InputAction::Press, 0);
	input.tick();
	EXPECT_EQ(input.getActionValue("jump"), 0.0f);
	EXPECT_EQ(input.getActionValue("fire"), 1.0f);
	EXPECT_TRUE(input.isMouseButtonDown(MOUSE_BUTTON_LEFT));
}

TEST(Input, OutOfRangeCodesAreIgnored) {
	FakeWindow window;
	Input input(&window);
	input.onKey(KEY_UNKNOWN, InputAction::Press, 0);
	input.onKey(10000, InputAction::Press, 0);
	input.onMouseButton(42, InputAction::Press, 0);
	EXPECT_FALSE(input.isKeyDown(KEY_UNKNOWN));
	EXPECT_FALSE(input.isKeyDown(10000));
	EXPECT_FALSE(input.isMouseButtonDown(42));
}

TEST(Input, MouseDeltaIsMeasuredBetweenTicks) {
	FakeWindow window;
	Input input(&window);
	input.onCursorMove(100.0, 50.0);
	input.tick();  // first tick never reports a jump
	EXPECT_EQ(input.getMouseDelta().x, 0.0f);
	EXPECT_EQ(input.getMousePosition().x, 100.0f);

	input.onCursorMove(110.0, 45.0);
	input.onCursorMove(130.0, 40.0);
	input.tick();
	EXPECT_EQ(input.getMouseDelta().x, 30.0f);
	EXPECT_EQ(input.getMouseDelta().y, -10.0f);
	input.tick();
	EXPECT_EQ(input.getMouseDelta().x, 0.0f);
}

TEST(Input, CaptureLocksCursorAndSuppressesTheJump) {
	FakeWindow window;
	Input input(&window);
	input.onCursorMove(10.0, 10.0);
	input.tick();
	input.setMouseCaptured(true);
	EXPECT_EQ(window.cursorMode, CursorMode::Disabled);
	EXPECT_TRUE(window.rawMotion);
	input.onCursorMove(500.0, 500.0);
	input.tick();
	EXPECT_EQ(input.getMouseDelta().x, 0.0f);
	input.setMouseCaptured(false);
	EXPECT_EQ(window.cursorMode, CursorMode::Normal);
	EXPECT_FALSE(window.rawMotion);
}

TEST(Input, MouseModeMapsCursorConstants) {
	FakeWindow window;
	Input input(&window);
	input.setMouseMode(CURSOR_HIDDEN);
	EXPECT_EQ(window.cursorMode, CursorMode::Hidden);
	input.setMouseMode(CURSOR_DISABLED);
	EXPECT_EQ(window.cursorMode, CursorMode::Disabled);
	input.setMouseMode(CURSOR_NORMAL);
	EXPECT_EQ(window.cursorMode, CursorMode::Normal);
}

TEST(Clock, IsMonotonic) {
	const double first = bird::platform::now();
	const double second = bird::platform::now();
	EXPECT_GE(second, first);
}

namespace {
std::string g_lastLog;
void captureLog(bird::platform::LogLevel, const std::string& message) {
	g_lastLog = message;
}
}  // namespace

TEST(Log, RoutesThroughTheInstalledSink) {
	bird::platform::setLogSink(captureLog);
	bird::platform::log(bird::platform::LogLevel::Info, "hello");
	bird::platform::setLogSink(nullptr);
	EXPECT_EQ(g_lastLog, "hello");
}

TEST(NativeVFS, ReadsWholeFilesAndReportsMissingOnes) {
	const auto path =
		std::filesystem::temp_directory_path() / "bird_vfs_test.bin";
	{
		std::ofstream out(path, std::ios::binary);
		out.write("\0\1\2\3", 4);
	}
	bird::platform::NativeVFS vfs;
	const auto bytes = vfs.readFile(path.string());
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(bytes->size(), 4u);
	EXPECT_EQ((*bytes)[3], 3u);
	EXPECT_TRUE(vfs.exists(path.string()));
	std::filesystem::remove(path);
	EXPECT_FALSE(vfs.readFile(path.string()).has_value());
	EXPECT_FALSE(vfs.exists(path.string()));
}

TEST(VFS, RejectsNullAndDefaultsToNative) {
	EXPECT_THROW(bird::platform::VFS::set(nullptr), std::invalid_argument);
	EXPECT_NE(dynamic_cast<const bird::platform::NativeVFS*>(
				  &bird::platform::VFS::get()),
			  nullptr);
}
