#include "CrashHandler.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "Logger.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#elif !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <unistd.h>
#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__)
#include <execinfo.h>
#endif
#endif

namespace fs = std::filesystem;

namespace bird::diag {

namespace {

std::mutex g_configMutex;
CrashHandlerConfig g_config;
std::atomic<bool> g_installed{false};
std::atomic<bool> g_inCrashHandler{false};
std::terminate_handler g_previousTerminate = nullptr;

// Fixed-size buffers populated at install() so POSIX signal handlers can build
// crash file paths without heap allocation.
char g_dumpDirBuf[512] = "crashes";
char g_prefixBuf[128] = "bird_crash";
bool g_flushOnCrash = true;

std::string formatFileTimestamp() {
	const auto now = std::chrono::system_clock::now();
	const std::time_t secs = std::chrono::system_clock::to_time_t(now);
	std::tm utc{};
#if defined(_WIN32)
	gmtime_s(&utc, &secs);
#else
	gmtime_r(&secs, &utc);
#endif
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
				  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
				  utc.tm_min, utc.tm_sec);
	return std::string(buf);
}

void copyToFixedBuf(char* dst, size_t dstSize, const std::string& src) {
	if (dstSize == 0) return;
	const size_t n = std::min(dstSize - 1, src.size());
	std::memcpy(dst, src.data(), n);
	dst[n] = '\0';
}

void birdTerminateHandler() {
	if (g_inCrashHandler.exchange(true)) {
		std::abort();
	}

	std::string reason = "std::terminate called";
	if (auto eptr = std::current_exception()) {
		try {
			std::rethrow_exception(eptr);
		} catch (const std::exception& e) {
			reason = std::string("Unhandled C++ exception: ") + e.what();
		} catch (...) {
			reason = "Unhandled unknown C++ exception";
		}
	}

	const StackTrace trace = captureStackTrace(1);
	Logger::get().fatal("Crash", reason + "\n" + trace.format());
	CrashHandler::writeCrashReport(reason, trace);

	if (g_previousTerminate) {
		g_previousTerminate();
	}
	std::abort();
}

#if defined(_WIN32)

LPTOP_LEVEL_EXCEPTION_FILTER g_previousExceptionFilter = nullptr;

LONG WINAPI birdUnhandledExceptionFilter(EXCEPTION_POINTERS* exInfo) {
	if (g_inCrashHandler.exchange(true)) {
		return EXCEPTION_EXECUTE_HANDLER;
	}

	const DWORD code =
		(exInfo && exInfo->ExceptionRecord)
			? exInfo->ExceptionRecord->ExceptionCode
			: 0;
	char reasonBuf[128];
	std::snprintf(reasonBuf, sizeof(reasonBuf),
				  "Unhandled Windows SEH Exception (0x%08lX)",
				  static_cast<unsigned long>(code));

	const CrashHandlerConfig cfg = CrashHandler::getConfig();
	const std::string stamp = formatFileTimestamp();

	if (cfg.writeMinidump && exInfo != nullptr) {
		std::error_code ec;
		fs::create_directories(cfg.dumpDirectory, ec);
		const fs::path dmpPath =
			fs::path(cfg.dumpDirectory) /
			(cfg.dumpFilePrefix + "_" + stamp + ".dmp");
		HANDLE hFile = CreateFileA(dmpPath.string().c_str(), GENERIC_WRITE, 0,
								   nullptr, CREATE_ALWAYS,
								   FILE_ATTRIBUTE_NORMAL, nullptr);
		if (hFile != INVALID_HANDLE_VALUE) {
			MINIDUMP_EXCEPTION_INFORMATION mei{};
			mei.ThreadId = GetCurrentThreadId();
			mei.ExceptionPointers = exInfo;
			mei.ClientPointers = FALSE;
			MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
							  hFile, MiniDumpNormal, &mei, nullptr, nullptr);
			CloseHandle(hFile);
		}
	}

	const StackTrace trace = captureStackTrace(1);
	CrashHandler::writeCrashReport(reasonBuf, trace);

	if (g_previousExceptionFilter) {
		return g_previousExceptionFilter(exInfo);
	}
	return EXCEPTION_EXECUTE_HANDLER;
}

#elif !defined(__EMSCRIPTEN__)

constexpr int kHandledSignals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS};
constexpr size_t kHandledSignalCount =
	sizeof(kHandledSignals) / sizeof(kHandledSignals[0]);
struct sigaction g_previousSigActions[kHandledSignalCount]{};

const char* signalName(int sig) {
	switch (sig) {
		case SIGSEGV:
			return "SIGSEGV (Segmentation Fault)";
		case SIGABRT:
			return "SIGABRT (Abort)";
		case SIGFPE:
			return "SIGFPE (Floating-Point Exception)";
		case SIGILL:
			return "SIGILL (Illegal Instruction)";
		case SIGBUS:
			return "SIGBUS (Bus Error)";
		default:
			return "UNKNOWN_SIGNAL";
	}
}

void writeStrFd(int fd, const char* str) {
	if (!str) return;
	size_t len = 0;
	while (str[len] != '\0') ++len;
	const ssize_t ignored = ::write(fd, str, len);
	(void)ignored;
}

void appendStr(char* dst, size_t dstSize, size_t& pos, const char* src) {
	if (!src) return;
	for (size_t i = 0; src[i] != '\0' && pos + 1 < dstSize; ++i) {
		dst[pos++] = src[i];
	}
	dst[pos] = '\0';
}

void appendInt(char* dst, size_t dstSize, size_t& pos, int value) {
	char tmp[16];
	size_t n = 0;
	if (value < 0) {
		if (pos + 1 < dstSize) dst[pos++] = '-';
		value = -value;
	}
	if (value == 0) {
		tmp[n++] = '0';
	} else {
		while (value > 0 && n < sizeof(tmp)) {
			tmp[n++] = static_cast<char>('0' + (value % 10));
			value /= 10;
		}
	}
	while (n > 0 && pos + 1 < dstSize) {
		dst[pos++] = tmp[--n];
	}
	dst[pos] = '\0';
}

void birdSignalHandler(int sig) {
	if (g_inCrashHandler.exchange(true)) {
		::signal(sig, SIG_DFL);
		::raise(sig);
		return;
	}

	// Build crash report path in stack buffer (async-signal-safe).
	char pathBuf[768];
	size_t pos = 0;
	pathBuf[0] = '\0';
	if (g_dumpDirBuf[0] != '\0') {
		appendStr(pathBuf, sizeof(pathBuf), pos, g_dumpDirBuf);
		appendStr(pathBuf, sizeof(pathBuf), pos, "/");
	}
	appendStr(pathBuf, sizeof(pathBuf), pos, g_prefixBuf);
	appendStr(pathBuf, sizeof(pathBuf), pos, "_signal_");
	appendInt(pathBuf, sizeof(pathBuf), pos, sig);
	appendStr(pathBuf, sizeof(pathBuf), pos, ".txt");

	const char* sigLabel = signalName(sig);
	const int fd =
		::open(pathBuf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		writeStrFd(fd, "=== Bird Engine Crash Report ===\nSignal: ");
		writeStrFd(fd, sigLabel);
		writeStrFd(fd, "\n\nStack Trace:\n");
#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__)
		void* addrs[64];
		const int count = ::backtrace(addrs, 64);
		if (count > 0) {
			::backtrace_symbols_fd(addrs, count, fd);
		}
#endif
		::close(fd);
	}

	writeStrFd(STDERR_FILENO, "\n[Bird CrashHandler] Fatal signal ");
	writeStrFd(STDERR_FILENO, sigLabel);
	writeStrFd(STDERR_FILENO, " — crash report written to: ");
	writeStrFd(STDERR_FILENO, pathBuf);
	writeStrFd(STDERR_FILENO, "\n");

	if (g_flushOnCrash) {
		Logger::get().flush();
	}

	// SA_RESETHAND restored default disposition; re-raise to exit cleanly.
	::raise(sig);
}

#endif

}  // namespace

void CrashHandler::install(const CrashHandlerConfig& config) {
	if (g_installed.load(std::memory_order_acquire)) {
		uninstall();
	}

	{
		std::lock_guard<std::mutex> lock(g_configMutex);
		g_config = config;
		copyToFixedBuf(g_dumpDirBuf, sizeof(g_dumpDirBuf),
					   config.dumpDirectory);
		copyToFixedBuf(g_prefixBuf, sizeof(g_prefixBuf), config.dumpFilePrefix);
		g_flushOnCrash = config.flushLogger;
	}

	if (!config.dumpDirectory.empty()) {
		std::error_code ec;
		fs::create_directories(config.dumpDirectory, ec);
	}

	g_inCrashHandler.store(false, std::memory_order_relaxed);
	g_previousTerminate = std::set_terminate(birdTerminateHandler);

#if defined(_WIN32)
	g_previousExceptionFilter =
		SetUnhandledExceptionFilter(birdUnhandledExceptionFilter);
#elif !defined(__EMSCRIPTEN__)
	struct sigaction sa{};
	sa.sa_handler = birdSignalHandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESETHAND;
	for (size_t i = 0; i < kHandledSignalCount; ++i) {
		::sigaction(kHandledSignals[i], &sa, &g_previousSigActions[i]);
	}
#endif

	g_installed.store(true, std::memory_order_release);
}

void CrashHandler::uninstall() {
	if (!g_installed.exchange(false, std::memory_order_acq_rel)) {
		return;
	}

	std::set_terminate(g_previousTerminate);
	g_previousTerminate = nullptr;

#if defined(_WIN32)
	SetUnhandledExceptionFilter(g_previousExceptionFilter);
	g_previousExceptionFilter = nullptr;
#elif !defined(__EMSCRIPTEN__)
	for (size_t i = 0; i < kHandledSignalCount; ++i) {
		::sigaction(kHandledSignals[i], &g_previousSigActions[i], nullptr);
	}
#endif
}

bool CrashHandler::isInstalled() {
	return g_installed.load(std::memory_order_acquire);
}

CrashHandlerConfig CrashHandler::getConfig() {
	std::lock_guard<std::mutex> lock(g_configMutex);
	return g_config;
}

std::string CrashHandler::writeCrashReport(const std::string& reason,
										   const StackTrace& trace) {
	const CrashHandlerConfig cfg = getConfig();
	if (cfg.flushLogger) {
		Logger::get().flush();
	}

	std::error_code ec;
	if (!cfg.dumpDirectory.empty()) {
		fs::create_directories(cfg.dumpDirectory, ec);
	}

	const std::string fileName =
		cfg.dumpFilePrefix + "_" + formatFileTimestamp() + ".txt";
	const fs::path reportPath =
		cfg.dumpDirectory.empty() ? fs::path(fileName)
								  : (fs::path(cfg.dumpDirectory) / fileName);

	std::ofstream out(reportPath, std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		return "";
	}

	out << "=== Bird Engine Crash Report ===\n";
	out << "Timestamp (UTC): " << formatFileTimestamp() << "\n";
	out << "Reason: " << reason << "\n";
	if (auto fileSink = Logger::get().getFileSink()) {
		out << "Active Log File: " << fileSink->filePath() << "\n";
	}
	out << "\nStack Trace:\n";
	out << trace.format();
	out.flush();
	out.close();

	if (cfg.onCrashCallback) {
		cfg.onCrashCallback(reportPath.string());
	}
	return reportPath.string();
}

}  // namespace bird::diag
