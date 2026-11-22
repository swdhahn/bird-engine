#ifndef BIRD_DIAGNOSTICS_STACK_TRACE_H
#define BIRD_DIAGNOSTICS_STACK_TRACE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bird::diag {

/**
 * Represents a single resolved frame in a captured native C++ call stack.
 */
struct StackFrame {
	/** Raw instruction pointer address, or 0 if unavailable. */
	uintptr_t address = 0;
	/** Demangled symbol / function name, or raw fallback label. */
	std::string symbol;
	/** Source file path if debug symbols are available (may be empty). */
	std::string file;
	/** 1-based source line number if debug symbols are available (0 otherwise). */
	uint32_t line = 0;
};

/**
 * Captured native C++ stack trace consisting of ordered frames from innermost
 * (caller) to outermost (thread entry point).
 */
struct StackTrace {
	/** Ordered call stack frames (index 0 is the most recent frame). */
	std::vector<StackFrame> frames;

	/** Returns true if no frames were captured. */
	bool empty() const { return frames.empty(); }

	/**
	 * Formats the stack trace as a multi-line human-readable report:
	 * `#0  0x...  bird::foo()  (file.cpp:42)`
	 *
	 * @return Formatted multi-line string.
	 */
	std::string format() const;
};

/**
 * Captures the current thread's native C++ call stack.
 *
 * Uses `cpptrace` when compiled with `BIRD_ENABLE_CPPTRACE`; otherwise falls
 * back to platform-native stack walking (`backtrace`/`dladdr`/`__cxa_demangle`
 * on Linux/macOS, `CaptureStackBackTrace`/`DbgHelp` on Windows).
 *
 * @param skipFrames Number of caller frames to omit above `captureStackTrace`.
 * @param maxFrames  Maximum number of frames to capture.
 * @return Populated StackTrace object.
 */
StackTrace captureStackTrace(size_t skipFrames = 0, size_t maxFrames = 64);

}  // namespace bird::diag

#endif	// BIRD_DIAGNOSTICS_STACK_TRACE_H
