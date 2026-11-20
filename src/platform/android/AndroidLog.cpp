#include "AndroidLog.h"

#include <android/log.h>
#include <unistd.h>

#include <cstdio>
#include <string>
#include <thread>

#include "../Log.h"

namespace bird::platform::android {

namespace {

constexpr const char* kTag = "Bird";

void logcatSink(LogLevel level, const std::string& message) {
	const int priority = level == LogLevel::Error	  ? ANDROID_LOG_ERROR
						 : level == LogLevel::Warning ? ANDROID_LOG_WARN
													  : ANDROID_LOG_INFO;
	__android_log_write(priority, kTag, message.c_str());
}

/** Forwards each line written to the pipe's write end into logcat. */
void pumpPipeToLogcat(int readEnd) {
	std::string line;
	char buffer[512];
	ssize_t count;
	while ((count = read(readEnd, buffer, sizeof(buffer))) > 0) {
		for (ssize_t i = 0; i < count; i++) {
			if (buffer[i] == '\n') {
				__android_log_write(ANDROID_LOG_INFO, kTag, line.c_str());
				line.clear();
			} else {
				line.push_back(buffer[i]);
			}
		}
	}
}

}  // namespace

void routeLoggingToLogcat() {
	static bool routed = false;
	if (routed) return;
	routed = true;
	setLogSink(logcatSink);

	int ends[2];
	if (pipe(ends) != 0) return;
	setvbuf(stdout, nullptr, _IOLBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);
	dup2(ends[1], STDOUT_FILENO);
	dup2(ends[1], STDERR_FILENO);
	std::thread(pumpPipeToLogcat, ends[0]).detach();
}

}  // namespace bird::platform::android
