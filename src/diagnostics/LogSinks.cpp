#include "LogSinks.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <utility>

namespace fs = std::filesystem;

namespace bird::diag {

namespace {

std::tm toUtcTm(std::time_t seconds) {
	std::tm utc{};
#if defined(_WIN32)
	gmtime_s(&utc, &seconds);
#else
	gmtime_r(&seconds, &utc);
#endif
	return utc;
}

const char* ansiColorForSeverity(LogSeverity severity) {
	switch (severity) {
		case LogSeverity::Trace:
			return "\033[90m";	// bright black / gray
		case LogSeverity::Debug:
			return "\033[36m";	// cyan
		case LogSeverity::Info:
			return "\033[32m";	// green
		case LogSeverity::Warn:
			return "\033[33m";	// yellow
		case LogSeverity::Error:
			return "\033[31m";	// red
		case LogSeverity::Fatal:
			return "\033[1;41;97m";	 // bold white on red background
	}
	return "";
}

constexpr const char* kAnsiReset = "\033[0m";

}  // namespace

const char* severityToString(LogSeverity severity) {
	switch (severity) {
		case LogSeverity::Trace:
			return "TRACE";
		case LogSeverity::Debug:
			return "DEBUG";
		case LogSeverity::Info:
			return "INFO";
		case LogSeverity::Warn:
			return "WARN";
		case LogSeverity::Error:
			return "ERROR";
		case LogSeverity::Fatal:
			return "FATAL";
	}
	return "UNKNOWN";
}

std::string LogRecord::formatTimestamp() const {
	const int64_t secs = epochMillis / 1000;
	int64_t ms = epochMillis % 1000;
	if (ms < 0) ms += 1000;
	const std::tm utc = toUtcTm(static_cast<std::time_t>(secs));
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
				  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
				  utc.tm_min, utc.tm_sec, static_cast<int>(ms));
	return std::string(buf);
}

std::string LogRecord::format() const {
	std::string out;
	out.reserve(64 + category.size() + message.size());
	out += '[';
	out += formatTimestamp();
	out += "] [";
	out += severityToString(severity);
	out += "] [";
	out += category.empty() ? "General" : category;
	out += "] ";
	out += message;
	return out;
}

// ============================================================================
// ConsoleLogSink
// ============================================================================

ConsoleLogSink::ConsoleLogSink(bool enableColors)
	: m_enableColors(enableColors) {}

void ConsoleLogSink::write(const LogRecord& record) {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::ostream& stream =
		static_cast<uint8_t>(record.severity) >=
				static_cast<uint8_t>(LogSeverity::Warn)
			? std::cerr
			: std::cout;
	const std::string line = record.format();
	if (m_enableColors) {
		stream << ansiColorForSeverity(record.severity) << line << kAnsiReset
			   << '\n';
	} else {
		stream << line << '\n';
	}
	if (static_cast<uint8_t>(record.severity) >=
		static_cast<uint8_t>(LogSeverity::Error)) {
		stream.flush();
	}
}

void ConsoleLogSink::flush() {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::cout.flush();
	std::cerr.flush();
}

void ConsoleLogSink::setColorsEnabled(bool enabled) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_enableColors = enabled;
}

bool ConsoleLogSink::areColorsEnabled() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_enableColors;
}

// ============================================================================
// RotatingFileLogSink
// ============================================================================

RotatingFileLogSink::RotatingFileLogSink(std::string filePath,
										 size_t maxFileSizeBytes,
										 size_t maxFiles)
	: m_filePath(std::move(filePath)),
	  m_maxFileSizeBytes(std::max<size_t>(1, maxFileSizeBytes)),
	  m_maxFiles(maxFiles) {
	std::lock_guard<std::mutex> lock(m_mutex);
	openLocked();
}

RotatingFileLogSink::~RotatingFileLogSink() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_stream.is_open()) {
		m_stream.flush();
		m_stream.close();
	}
}

std::string RotatingFileLogSink::rotatedPath(size_t index) const {
	const fs::path p(m_filePath);
	const fs::path dir = p.parent_path();
	const std::string stem = p.stem().string();
	const std::string ext = p.extension().string();
	const std::string name = stem + "." + std::to_string(index) + ext;
	return dir.empty() ? name : (dir / name).string();
}

void RotatingFileLogSink::openLocked() {
	if (m_filePath.empty()) return;
	std::error_code ec;
	const fs::path p(m_filePath);
	if (p.has_parent_path()) {
		fs::create_directories(p.parent_path(), ec);
	}
	m_stream.open(m_filePath, std::ios::out | std::ios::app);
	if (m_stream.is_open()) {
		const auto sz = fs::file_size(p, ec);
		m_currentSize = ec ? 0 : static_cast<size_t>(sz);
	} else {
		m_currentSize = 0;
	}
}

void RotatingFileLogSink::rotateLocked() {
	if (m_stream.is_open()) {
		m_stream.flush();
		m_stream.close();
	}

	std::error_code ec;
	if (m_maxFiles == 0) {
		// Truncate active file in place when no backup archives are kept.
		m_stream.open(m_filePath, std::ios::out | std::ios::trunc);
		m_currentSize = 0;
		return;
	}

	// Remove the oldest archive if it exists.
	fs::remove(rotatedPath(m_maxFiles), ec);

	// Shift archives: (maxFiles - 1) -> maxFiles, ..., 1 -> 2.
	for (size_t i = m_maxFiles; i > 1; --i) {
		const std::string src = rotatedPath(i - 1);
		const std::string dst = rotatedPath(i);
		if (fs::exists(src, ec)) {
			fs::rename(src, dst, ec);
		}
	}

	// Move active file to .1 archive.
	if (fs::exists(m_filePath, ec)) {
		fs::rename(m_filePath, rotatedPath(1), ec);
	}

	m_stream.open(m_filePath, std::ios::out | std::ios::trunc);
	m_currentSize = 0;
}

void RotatingFileLogSink::write(const LogRecord& record) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_stream.is_open()) return;

	const std::string line = record.format() + "\n";
	if (m_currentSize > 0 &&
		m_currentSize + line.size() > m_maxFileSizeBytes) {
		rotateLocked();
		if (!m_stream.is_open()) return;
	}

	m_stream.write(line.data(), static_cast<std::streamsize>(line.size()));
	m_currentSize += line.size();

	if (static_cast<uint8_t>(record.severity) >=
		static_cast<uint8_t>(LogSeverity::Error)) {
		m_stream.flush();
	}
}

void RotatingFileLogSink::flush() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_stream.is_open()) {
		m_stream.flush();
	}
}

bool RotatingFileLogSink::isOpen() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_stream.is_open();
}

size_t RotatingFileLogSink::currentFileSize() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_currentSize;
}

// ============================================================================
// RingBufferLogSink
// ============================================================================

RingBufferLogSink::RingBufferLogSink(size_t capacity)
	: m_buffer(std::max<size_t>(1, capacity)),
	  m_capacity(std::max<size_t>(1, capacity)) {}

void RingBufferLogSink::write(const LogRecord& record) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_buffer[m_head] = record;
	m_head = (m_head + 1) % m_capacity;
	if (m_count < m_capacity) {
		++m_count;
	}
}

std::vector<LogRecord> RingBufferLogSink::snapshot(size_t maxCount) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return {};

	const size_t resultCount =
		(maxCount == 0 || maxCount >= m_count) ? m_count : maxCount;
	std::vector<LogRecord> out;
	out.reserve(resultCount);

	const size_t oldest = (m_head + m_capacity - m_count) % m_capacity;
	const size_t skip = m_count - resultCount;
	for (size_t i = 0; i < resultCount; ++i) {
		out.push_back(m_buffer[(oldest + skip + i) % m_capacity]);
	}
	return out;
}

std::vector<LogRecord> RingBufferLogSink::snapshotSince(
	uint64_t afterSequence) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return {};

	std::vector<LogRecord> out;
	const size_t oldest = (m_head + m_capacity - m_count) % m_capacity;
	for (size_t i = 0; i < m_count; ++i) {
		const LogRecord& rec = m_buffer[(oldest + i) % m_capacity];
		if (rec.sequence > afterSequence) {
			out.push_back(rec);
		}
	}
	return out;
}

std::vector<LogRecord> RingBufferLogSink::snapshotFiltered(
	LogSeverity minSeverity, const std::string& categoryFilter) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return {};

	std::vector<LogRecord> out;
	const size_t oldest = (m_head + m_capacity - m_count) % m_capacity;
	for (size_t i = 0; i < m_count; ++i) {
		const LogRecord& rec = m_buffer[(oldest + i) % m_capacity];
		if (static_cast<uint8_t>(rec.severity) <
			static_cast<uint8_t>(minSeverity)) {
			continue;
		}
		if (!categoryFilter.empty() && rec.category != categoryFilter) {
			continue;
		}
		out.push_back(rec);
	}
	return out;
}

uint64_t RingBufferLogSink::latestSequence() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return 0;
	const size_t newest = (m_head + m_capacity - 1) % m_capacity;
	return m_buffer[newest].sequence;
}

size_t RingBufferLogSink::size() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_count;
}

void RingBufferLogSink::clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_head = 0;
	m_count = 0;
}

}  // namespace bird::diag
