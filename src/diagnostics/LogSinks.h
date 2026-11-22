#ifndef BIRD_DIAGNOSTICS_LOG_SINKS_H
#define BIRD_DIAGNOSTICS_LOG_SINKS_H

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace bird::diag {

/**
 * Severity levels for structured engine and script log messages.
 * Ordered from most verbose (Trace) to most critical (Fatal).
 */
enum class LogSeverity : uint8_t {
	Trace = 0,
	Debug = 1,
	Info = 2,
	Warn = 3,
	Error = 4,
	Fatal = 5
};

/**
 * Converts a LogSeverity value to its uppercase printable label
 * (e.g. "TRACE", "INFO", "ERROR").
 *
 * @param severity The severity level to convert.
 * @return Null-terminated static string literal.
 */
const char* severityToString(LogSeverity severity);

/**
 * Immutable payload describing a single structured log entry.
 *
 * Captures wall-clock timestamp, monotonic sequence number, severity,
 * subsystem category tag, message body, thread identifier, and optional
 * C++ source location.
 */
struct LogRecord {
	/** Monotonically increasing sequence number assigned by Logger. */
	uint64_t sequence = 0;
	/** UTC wall-clock timestamp in milliseconds since Unix epoch. */
	int64_t epochMillis = 0;
	/** Severity level of the entry. */
	LogSeverity severity = LogSeverity::Info;
	/** Subsystem or module category tag (e.g. "Renderer", "Physics", "Dart"). */
	std::string category;
	/** Human-readable log message body. */
	std::string message;
	/** Hashed identifier of the thread that emitted this record. */
	uint64_t threadId = 0;
	/** Optional source file path (empty when not supplied). */
	std::string file;
	/** Optional 1-based source line number (0 when not supplied). */
	int line = 0;

	/**
	 * Formats the record as a single human-readable line:
	 * `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [Category] message`
	 *
	 * @return Formatted log line without a trailing newline.
	 */
	std::string format() const;

	/**
	 * Formats `epochMillis` as a UTC timestamp string `YYYY-MM-DD HH:MM:SS.mmm`.
	 *
	 * @return Formatted UTC timestamp string.
	 */
	std::string formatTimestamp() const;
};

/**
 * Abstract destination for structured log records.
 *
 * Implementations must be safe to call concurrently from multiple threads
 * (either via internal synchronization or when invoked under Logger's lock).
 */
class ILogSink {
   public:
	virtual ~ILogSink() = default;

	/**
	 * Consumes a single log record.
	 *
	 * @param record The structured log entry to process.
	 */
	virtual void write(const LogRecord& record) = 0;

	/**
	 * Flushes any buffered output to the underlying medium.
	 */
	virtual void flush() = 0;
};

/**
 * Log sink that writes formatted entries to standard output and standard error.
 *
 * Routes Trace, Debug, and Info messages to stdout, and Warn, Error, and Fatal
 * messages to stderr. Optionally emits ANSI color escape sequences.
 */
class ConsoleLogSink final : public ILogSink {
   public:
	/**
	 * Constructs a console sink.
	 *
	 * @param enableColors When true, wraps log lines in ANSI severity colors
	 *        if stdout and stderr are terminals (never in browsers or logcat).
	 */
	explicit ConsoleLogSink(bool enableColors = true);

	void write(const LogRecord& record) override;
	void flush() override;

	/** Enables or disables ANSI color codes at runtime. */
	void setColorsEnabled(bool enabled);
	bool areColorsEnabled() const;

   private:
	mutable std::mutex m_mutex;
	bool m_enableColors;
};

/**
 * Log sink that writes formatted entries to a size-bounded rotating `.log` file.
 *
 * When the active log file reaches `maxFileSizeBytes`, existing files are
 * rotated (`base.log` -> `base.1.log` -> `base.2.log`, up to `maxFiles`) and
 * a fresh active file is opened. Automatically flushes on `Error` and `Fatal`
 * records as well as on `flush()` and destruction.
 */
class RotatingFileLogSink final : public ILogSink {
   public:
	/** Default maximum size per log file before rotation (5 MiB). */
	static constexpr size_t kDefaultMaxFileSizeBytes = 5 * 1024 * 1024;
	/** Default number of rotated backup files retained alongside the active file. */
	static constexpr size_t kDefaultMaxFiles = 5;

	/**
	 * Opens (or creates) the rotating log file at `filePath`.
	 * Parent directories are created automatically if missing.
	 *
	 * @param filePath Path to the active `.log` file (e.g. `"logs/bird.log"`).
	 * @param maxFileSizeBytes Size threshold in bytes that triggers rotation.
	 * @param maxFiles Maximum number of rotated archive files to keep.
	 */
	explicit RotatingFileLogSink(
		std::string filePath,
		size_t maxFileSizeBytes = kDefaultMaxFileSizeBytes,
		size_t maxFiles = kDefaultMaxFiles);
	~RotatingFileLogSink() override;

	void write(const LogRecord& record) override;
	void flush() override;

	/** Returns true if the active log file stream is currently open and healthy. */
	bool isOpen() const;

	/** Returns the path to the active log file. */
	const std::string& filePath() const { return m_filePath; }

	/** Returns the number of bytes written to the current active file. */
	size_t currentFileSize() const;

   private:
	void openLocked();
	void rotateLocked();
	std::string rotatedPath(size_t index) const;

	mutable std::mutex m_mutex;
	std::string m_filePath;
	size_t m_maxFileSizeBytes;
	size_t m_maxFiles;
	size_t m_currentSize = 0;
	std::ofstream m_stream;
};

/**
 * Thread-safe fixed-capacity in-memory ring buffer sink for in-game UI consoles.
 *
 * Retains the most recent `capacity` log records in chronological order.
 * Overwrites the oldest entry when full, avoiding unbounded memory growth.
 */
class RingBufferLogSink final : public ILogSink {
   public:
	/** Default number of recent log records retained in memory. */
	static constexpr size_t kDefaultCapacity = 2048;

	/**
	 * Constructs a ring buffer sink with the specified capacity.
	 *
	 * @param capacity Maximum number of records retained (clamped to >= 1).
	 */
	explicit RingBufferLogSink(size_t capacity = kDefaultCapacity);

	void write(const LogRecord& record) override;
	void flush() override {}

	/**
	 * Returns a chronological copy of buffered records.
	 *
	 * @param maxCount Maximum number of most-recent records to return (0 = all).
	 * @return Vector of LogRecords ordered oldest to newest.
	 */
	std::vector<LogRecord> snapshot(size_t maxCount = 0) const;

	/**
	 * Returns all buffered records with `sequence > afterSequence`.
	 * Useful for incremental UI log console updates without copying old entries.
	 *
	 * @param afterSequence Last sequence number already consumed by the caller.
	 * @return Newly arrived records ordered oldest to newest.
	 */
	std::vector<LogRecord> snapshotSince(uint64_t afterSequence) const;

	/**
	 * Returns buffered records matching a minimum severity and optional
	 * substring category filter.
	 *
	 * @param minSeverity Minimum severity level to include.
	 * @param categoryFilter Optional exact category name (empty = all categories).
	 * @return Matching records ordered oldest to newest.
	 */
	std::vector<LogRecord> snapshotFiltered(
		LogSeverity minSeverity,
		const std::string& categoryFilter = "") const;

	/** Returns the sequence number of the newest stored record, or 0 if empty. */
	uint64_t latestSequence() const;

	/** Returns the current number of stored records (<= capacity()). */
	size_t size() const;

	/** Returns the maximum number of records this buffer can hold. */
	size_t capacity() const { return m_capacity; }

	/** Removes all buffered records. */
	void clear();

   private:
	mutable std::mutex m_mutex;
	std::vector<LogRecord> m_buffer;
	size_t m_capacity;
	size_t m_head = 0;	// next write index
	size_t m_count = 0;
};

}  // namespace bird::diag

#endif	// BIRD_DIAGNOSTICS_LOG_SINKS_H
