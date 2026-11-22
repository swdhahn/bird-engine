#ifndef BIRD_ANDROIDLOG_H
#define BIRD_ANDROIDLOG_H

namespace bird::platform::android {

/**
 * Sends platform::log lines and everything written to stdout and stderr
 * (the engine's std::cout diagnostics) to logcat under the tag "Bird".
 * Idempotent; call once at startup.
 */
void routeLoggingToLogcat();

}  // namespace bird::platform::android

#endif	// BIRD_ANDROIDLOG_H
