#ifndef BIRD_DIAGNOSTICS_CRASH_HANDLER_H
#define BIRD_DIAGNOSTICS_CRASH_HANDLER_H

#include <functional>
#include <string>

#include "StackTrace.h"

namespace bird::diag {

/**
 * Configuration for the OS-level crash handler and crash report writer.
 */
struct CrashHandlerConfig {
	/** Directory where crash dumps and stack trace reports are written. */
	std::string dumpDirectory = "crashes";
	/** Filename prefix for generated crash reports and minidumps. */
	std::string dumpFilePrefix = "bird_crash";
	/**
	 * When true on Windows, writes a binary `.dmp` via `MiniDumpWriteDump`
	 * alongside the text stack trace report.
	 */
	bool writeMinidump = true;
	/**
	 * When true, flushes `bird::diag::Logger::get()` when a crash or
	 * unhandled termination occurs so buffered log lines reach disk.
	 */
	bool flushLogger = true;
	/**
	 * Optional callback invoked after `writeCrashReport()` writes a report
	 * on normal C++ error/terminate paths. Not invoked from POSIX signal
	 * handlers to preserve async-signal safety.
	 */
	std::function<void(const std::string& reportPath)> onCrashCallback;
};

/**
 * Installs OS-level crash handlers and C++ terminate hooks that flush logs
 * and persist a crash dump / stack trace report to disk.
 *
 * Platform behavior:
 * - **Windows**: Installs `SetUnhandledExceptionFilter` to write both a
 *   `MiniDumpWriteDump` `.dmp` file and a readable `.txt` stack trace report.
 * - **Linux / macOS**: Installs `sigaction` handlers for `SIGSEGV`, `SIGABRT`,
 *   `SIGFPE`, `SIGILL`, and `SIGBUS` using `SA_RESETHAND`, writing an
 *   async-signal-safe crash report with backtrace symbols before re-raising
 *   the signal.
 * - **All platforms**: Installs a `std::set_terminate` handler that captures
 *   a full stack trace, logs a `FATAL` entry, writes a `.txt` crash report,
 *   and flushes the logger.
 */
class CrashHandler {
   public:
	/**
	 * Installs OS crash handlers and the C++ terminate handler.
	 * Creates `config.dumpDirectory` immediately so the signal handler never
	 * has to create directories during a crash.
	 *
	 * @param config Crash handler configuration.
	 */
	static void install(const CrashHandlerConfig& config = CrashHandlerConfig{});

	/**
	 * Restores the previous OS crash handlers and C++ terminate handler.
	 */
	static void uninstall();

	/** Returns true if `install()` is currently active. */
	static bool isInstalled();

	/**
	 * Writes a formatted text crash report containing the timestamp, reason,
	 * active log file path (if any), and captured C++ stack trace to disk.
	 *
	 * Also flushes `Logger::get()` if `config.flushLogger` is enabled.
	 * Safe to call directly on fatal errors or in unit tests without
	 * terminating the process.
	 *
	 * @param reason Short description of the fault or exception.
	 * @param trace  Captured native stack trace.
	 * @return Path to the written crash report file, or empty string on I/O failure.
	 */
	static std::string writeCrashReport(const std::string& reason,
										const StackTrace& trace);

	/** Returns the currently active configuration. */
	static CrashHandlerConfig getConfig();
};

}  // namespace bird::diag

#endif	// BIRD_DIAGNOSTICS_CRASH_HANDLER_H
