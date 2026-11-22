#include "Log.h"

#include <atomic>
#include <iostream>

namespace bird::platform {

namespace {

void consoleSink(LogLevel level, const std::string& message) {
	std::ostream& stream = level == LogLevel::Info ? std::cout : std::cerr;
	stream << message << std::endl;
}

std::atomic<LogSink> g_sink{consoleSink};

}  // namespace

void log(LogLevel level, const std::string& message) {
	g_sink.load()(level, message);
}

void setLogSink(LogSink sink) {
	g_sink.store(sink ? sink : consoleSink);
}

}  // namespace bird::platform
