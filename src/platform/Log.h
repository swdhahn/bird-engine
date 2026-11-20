#ifndef BIRD_LOG_H
#define BIRD_LOG_H

#include <string>

namespace bird::platform {

enum class LogLevel { Info, Warning, Error };

/** Receives every log line; platforms install one (e.g. logcat). */
using LogSink = void (*)(LogLevel level, const std::string& message);

/**
 * Writes one line through the active sink. The default sink prints Info to
 * standard output and Warning or Error to standard error.
 */
void log(LogLevel level, const std::string& message);

/** Replaces the sink; nullptr restores the default. */
void setLogSink(LogSink sink);

}  // namespace bird::platform

#endif	// BIRD_LOG_H
