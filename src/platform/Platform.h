#ifndef BIRD_PLATFORM_H
#define BIRD_PLATFORM_H

#include <cstdint>
#include <memory>
#include <string>

#include "../renders/Window.h"

/*
 * Entry points every platform backend implements. The backend is chosen at
 * build time (CMake option BIRD_PLATFORM); exactly one implementation of
 * these functions is linked:
 *   DESKTOP   src/platform/desktop   (GLFW; Linux, Windows, macOS)
 * Android, iOS and Web backends plug in here.
 */
namespace bird::platform {

/**
 * Creates the application window with a current graphics context for
 * CURRENT_GRAPHICS_PIPELINE.
 *
 * @param width Requested width in screen points (platforms may ignore it).
 * @param height Requested height in screen points (platforms may ignore it).
 * @param title Caption, where the platform shows one.
 * @throws std::runtime_error when the window or context cannot be created.
 */
std::unique_ptr<Window> createWindow(uint32_t width, uint32_t height,
									 const std::string& title);

}  // namespace bird::platform

#endif	// BIRD_PLATFORM_H
