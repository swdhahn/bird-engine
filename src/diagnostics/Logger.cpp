#include "Logger.h"

#include <chrono>
#include <functional>
#include <thread>

#include "../platform/Log.h"

namespace bird::diag {

namespace {

int64_t currentEpochMillis() {
	const auto now = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   now.time_since_epoch())
		.count();
}

uint64_t currentThreadHash() {
	return static_cast<uint64_t>(
		std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

void platformLogBridge(bird::platform::LogLevel level,
					   const std::string& message) {
	LogSeverity sev = LogSeverity::Info;
	switch (level) {
		case bird::platform::LogLevel::Info:
			sev = LogSeverity::Info;
			break;
		case bird::platform::LogLevel::Warning:
			sev = LogSeverity::Warn;
			break;
		case bird::platform::LogLevel::Error:
			sev = LogSeverity::Error;
			break;
	}
	Logger::get().log(sev, "Platform", message);
}

}  // namespace

Logger& Logger::get() {
	static Logger instance;
	return instance;
}

Logger::Logger() {
	// Default lightweight setup so logging before explicit init() still works.
	m_ringBufferSink = std::make_shared<RingBufferLogSink>();
	m_sinks.push_back(std::make_shared<ConsoleLogSink>(true));
	m_sinks.push_back(m_ringBufferSink);
}

Logger::~Logger() {
	shutdown();
}

void Logger::init(const LoggerConfig& config) {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& sink : m_sinks) {
		if (sink) sink->flush();
	}
	m_sinks.clear();
	m_ringBufferSink.reset();
	m_fileSink.reset();

	m_minSeverity.store(config.minSeverity, std::memory_order_relaxed);

	if (config.enableConsoleSink) {
		m_sinks.push_back(
			std::make_shared<ConsoleLogSink>(config.enableConsoleColors));
	}
	if (config.enableRingBufferSink) {
		m_ringBufferSink =
			std::make_shared<RingBufferLogSink>(config.ringBufferCapacity);
		m_sinks.push_back(m_ringBufferSink);
	}
	if (!config.logFilePath.empty()) {
		m_fileSink = std::make_shared<RotatingFileLogSink>(
			config.logFilePath, config.maxFileSizeBytes, config.maxLogFiles);
		m_sinks.push_back(m_fileSink);
	}

	if (config.bridgePlatformLog) {
		bird::platform::setLogSink(platformLogBridge);
		m_bridgedPlatformLog = true;
	} else if (m_bridgedPlatformLog) {
		bird::platform::setLogSink(nullptr);
		m_bridgedPlatformLog = false;
	}

	m_initialized.store(true, std::memory_order_release);
}

void Logger::shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& sink : m_sinks) {
		if (sink) sink->flush();
	}
	if (m_bridgedPlatformLog) {
		bird::platform::setLogSink(nullptr);
		m_bridgedPlatformLog = false;
	}
	m_sinks.clear();
	m_fileSink.reset();
	m_ringBufferSink.reset();
	m_initialized.store(false, std::memory_order_release);
}

bool Logger::isInitialized() const {
	return m_initialized.load(std::memory_order_acquire);
}

void Logger::log(LogSeverity severity, const std::string& category,
				 const std::string& message, const char* file, int line) {
	if (static_cast<uint8_t>(severity) <
		static_cast<uint8_t>(m_minSeverity.load(std::memory_order_relaxed))) {
		return;
	}

	LogRecord record;
	record.sequence = m_nextSequence.fetch_add(1, std::memory_order_relaxed);
	record.epochMillis = currentEpochMillis();
	record.severity = severity;
	record.category = category;
	record.message = message;
	record.threadId = currentThreadHash();
	if (file != nullptr) record.file = file;
	record.line = line;

	std::vector<std::shared_ptr<ILogSink>> sinksCopy;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		sinksCopy = m_sinks;
	}

	const bool shouldFlush =
		static_cast<uint8_t>(severity) >=
		static_cast<uint8_t>(LogSeverity::Error);
	for (const auto& sink : sinksCopy) {
		if (!sink) continue;
		sink->write(record);
		if (shouldFlush) sink->flush();
	}
}

void Logger::trace(const std::string& category, const std::string& message,
				   const char* file, int line) {
	log(LogSeverity::Trace, category, message, file, line);
}

void Logger::debug(const std::string& category, const std::string& message,
				   const char* file, int line) {
	log(LogSeverity::Debug, category, message, file, line);
}

void Logger::info(const std::string& category, const std::string& message,
				  const char* file, int line) {
	log(LogSeverity::Info, category, message, file, line);
}

void Logger::warn(const std::string& category, const std::string& message,
				  const char* file, int line) {
	log(LogSeverity::Warn, category, message, file, line);
}

void Logger::error(const std::string& category, const std::string& message,
				   const char* file, int line) {
	log(LogSeverity::Error, category, message, file, line);
}

void Logger::fatal(const std::string& category, const std::string& message,
				   const char* file, int line) {
	log(LogSeverity::Fatal, category, message, file, line);
}

void Logger::flush() {
	std::vector<std::shared_ptr<ILogSink>> sinksCopy;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		sinksCopy = m_sinks;
	}
	for (const auto& sink : sinksCopy) {
		if (sink) sink->flush();
	}
}

void Logger::setMinSeverity(LogSeverity severity) {
	m_minSeverity.store(severity, std::memory_order_relaxed);
}

LogSeverity Logger::getMinSeverity() const {
	return m_minSeverity.load(std::memory_order_relaxed);
}

void Logger::addSink(std::shared_ptr<ILogSink> sink) {
	if (!sink) return;
	std::lock_guard<std::mutex> lock(m_mutex);
	m_sinks.push_back(std::move(sink));
}

void Logger::clearSinks() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& sink : m_sinks) {
		if (sink) sink->flush();
	}
	m_sinks.clear();
	m_ringBufferSink.reset();
	m_fileSink.reset();
}

std::shared_ptr<RingBufferLogSink> Logger::getRingBufferSink() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_ringBufferSink;
}

std::shared_ptr<RotatingFileLogSink> Logger::getFileSink() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_fileSink;
}

}  // namespace bird::diag
