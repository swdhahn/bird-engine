#ifndef BIRD_DIAGNOSTICS_LOGGER_H
#define BIRD_DIAGNOSTICS_LOGGER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "LogSinks.h"

namespace bird::diag {

/**
 * Configuration options for initializing the global structured Logger.
 */
struct LoggerConfig {
	/** Minimum severity accepted by the logger (records below this are dropped). */
	LogSeverity minSeverity = LogSeverity::Debug;
	/** Attach a ConsoleLogSink writing to stdout/stderr. */
	bool enableConsoleSink = true;
	/** Enable ANSI color codes on the console sink. */
	bool enableConsoleColors = true;
	/** Attach a RingBufferLogSink for in-game UI log consoles. */
	bool enableRingBufferSink = true;
	/** Number of entries retained in the in-memory ring buffer. */
	size_t ringBufferCapacity = RingBufferLogSink::kDefaultCapacity;
	/**
	 * Path to the rotating `.log` file on disk.
	 * Leave empty to disable file logging.
	 */
	std::string logFilePath;
	/** Maximum size per log file in bytes before rotating. */
	size_t maxFileSizeBytes = RotatingFileLogSink::kDefaultMaxFileSizeBytes;
	/** Maximum number of rotated archive files to keep on disk. */
	size_t maxLogFiles = RotatingFileLogSink::kDefaultMaxFiles;
	/**
	 * When true, installs a bridge sink on `bird::platform::setLogSink` so
	 * legacy `bird::platform::log()` calls flow into this Logger.
	 */
	bool bridgePlatformLog = true;
};

/**
 * Thread-safe, multi-sink structured logger singleton for Bird Engine.
 *
 * Supports severity filtering (`Trace` through `Fatal`), UTC timestamps,
 * subsystem category tags, and pluggable sinks (console, rotating file,
 * in-memory ring buffer, and custom user sinks).
 *
 * Usage:
 * @code
 * bird::diag::LoggerConfig cfg;
 * cfg.logFilePath = "logs/bird.log";
 * bird::diag::Logger::get().init(cfg);
 *
 * BIRD_LOG_INFO("Renderer", "OpenGL context initialized");
 * BIRD_LOG_ERROR("Assets", "Missing texture: hero.png");
 * @endcode
 */
class Logger {
   public:
	/** Returns the process-wide Logger instance. */
	static Logger& get();

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	/**
	 * Configures and activates the logger with the requested sinks.
	 * Safe to call again to reconfigure sinks (previous sinks are flushed
	 * and replaced).
	 *
	 * @param config Logger configuration options.
	 */
	void init(const LoggerConfig& config = LoggerConfig{});

	/**
	 * Flushes all active sinks, releases the rotating file sink, and removes
	 * the platform log bridge if installed.
	 */
	void shutdown();

	/** Returns true if init() has been called and shutdown() has not. */
	bool isInitialized() const;

	/**
	 * Emits a structured log record to all registered sinks if `severity`
	 * meets or exceeds the configured minimum severity.
	 *
	 * Automatically flushes all sinks when `severity` is `Error` or `Fatal`.
	 *
	 * @param severity Severity level of the message.
	 * @param category Subsystem tag (e.g. `"Engine"`, `"Physics"`, `"Dart"`).
	 * @param message Human-readable log message.
	 * @param file Optional source file path (typically `__FILE__`).
	 * @param line Optional source line number (typically `__LINE__`).
	 */
	void log(LogSeverity severity, const std::string& category,
			 const std::string& message, const char* file = nullptr,
			 int line = 0);

	/** Convenience overload for Trace-level messages. */
	void trace(const std::string& category, const std::string& message,
			   const char* file = nullptr, int line = 0);
	/** Convenience overload for Debug-level messages. */
	void debug(const std::string& category, const std::string& message,
			   const char* file = nullptr, int line = 0);
	/** Convenience overload for Info-level messages. */
	void info(const std::string& category, const std::string& message,
			  const char* file = nullptr, int line = 0);
	/** Convenience overload for Warn-level messages. */
	void warn(const std::string& category, const std::string& message,
			  const char* file = nullptr, int line = 0);
	/** Convenience overload for Error-level messages. */
	void error(const std::string& category, const std::string& message,
			   const char* file = nullptr, int line = 0);
	/** Convenience overload for Fatal-level messages. */
	void fatal(const std::string& category, const std::string& message,
			   const char* file = nullptr, int line = 0);

	/** Flushes all registered sinks immediately. */
	void flush();

	/** Sets the minimum severity threshold (lock-free atomic update). */
	void setMinSeverity(LogSeverity severity);

	/** Returns the current minimum severity threshold. */
	LogSeverity getMinSeverity() const;

	/**
	 * Registers an additional custom sink to receive subsequent log records.
	 *
	 * @param sink Shared pointer to the sink; ignored if null.
	 */
	void addSink(std::shared_ptr<ILogSink> sink);

	/** Removes all registered sinks after flushing them. */
	void clearSinks();

	/**
	 * Returns the built-in in-memory ring buffer sink, or nullptr if disabled.
	 * Used by the engine's custom UI to render an in-game log console.
	 */
	std::shared_ptr<RingBufferLogSink> getRingBufferSink() const;

	/**
	 * Returns the built-in rotating file sink, or nullptr if file logging is
	 * disabled.
	 */
	std::shared_ptr<RotatingFileLogSink> getFileSink() const;

   private:
	Logger();
	~Logger();

	mutable std::mutex m_mutex;
	std::vector<std::shared_ptr<ILogSink>> m_sinks;
	std::shared_ptr<RingBufferLogSink> m_ringBufferSink;
	std::shared_ptr<RotatingFileLogSink> m_fileSink;
	std::atomic<LogSeverity> m_minSeverity{LogSeverity::Debug};
	std::atomic<uint64_t> m_nextSequence{1};
	std::atomic<bool> m_initialized{false};
	bool m_bridgedPlatformLog = false;
};

}  // namespace bird::diag

/** Convenience logging macros that capture source file and line metadata. */
#define BIRD_LOG_TRACE(category, message)                                    \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Trace,        \
									(category), (message), __FILE__, __LINE__)
#define BIRD_LOG_DEBUG(category, message)                                    \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Debug,        \
									(category), (message), __FILE__, __LINE__)
#define BIRD_LOG_INFO(category, message)                                     \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Info,         \
									(category), (message), __FILE__, __LINE__)
#define BIRD_LOG_WARN(category, message)                                     \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Warn,         \
									(category), (message), __FILE__, __LINE__)
#define BIRD_LOG_ERROR(category, message)                                    \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Error,        \
									(category), (message), __FILE__, __LINE__)
#define BIRD_LOG_FATAL(category, message)                                    \
	::bird::diag::Logger::get().log(::bird::diag::LogSeverity::Fatal,        \
									(category), (message), __FILE__, __LINE__)

#endif	// BIRD_DIAGNOSTICS_LOGGER_H
