#include "StackTrace.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#if defined(BIRD_ENABLE_CPPTRACE)
#include <cpptrace/cpptrace.hpp>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <mutex>
#elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__)
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif

namespace bird::diag {

std::string StackTrace::format() const {
	if (frames.empty()) {
		return "  <no stack frames captured>\n";
	}
	std::ostringstream oss;
	for (size_t i = 0; i < frames.size(); ++i) {
		const StackFrame& f = frames[i];
		char addrBuf[32];
		std::snprintf(addrBuf, sizeof(addrBuf), "0x%016llx",
					  static_cast<unsigned long long>(f.address));
		oss << "  #" << i << "  " << addrBuf << "  "
			<< (f.symbol.empty() ? "<unknown>" : f.symbol);
		if (!f.file.empty()) {
			oss << "  (" << f.file;
			if (f.line > 0) {
				oss << ':' << f.line;
			}
			oss << ')';
		}
		oss << '\n';
	}
	return oss.str();
}

StackTrace captureStackTrace(size_t skipFrames, size_t maxFrames) {
	StackTrace trace;
	if (maxFrames == 0) return trace;

#if defined(BIRD_ENABLE_CPPTRACE)
	const auto raw = cpptrace::generate_trace(skipFrames + 1, maxFrames);
	trace.frames.reserve(raw.frames.size());
	for (const auto& f : raw.frames) {
		StackFrame out;
		out.address = static_cast<uintptr_t>(f.raw_address);
		out.symbol = f.symbol;
		out.file = f.filename;
		out.line = f.line.value_or(0);
		trace.frames.push_back(std::move(out));
	}

#elif defined(_WIN32)
	static std::mutex symMutex;
	std::lock_guard<std::mutex> lock(symMutex);

	HANDLE process = GetCurrentProcess();
	static bool symInitialized = false;
	if (!symInitialized) {
		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS |
					  SYMOPT_LOAD_LINES);
		symInitialized = (SymInitialize(process, nullptr, TRUE) == TRUE);
	}

	const DWORD framesToCapture =
		static_cast<DWORD>(maxFrames > 62 ? 62 : maxFrames);
	std::vector<void*> addrs(framesToCapture);
	const USHORT captured = CaptureStackBackTrace(
		static_cast<DWORD>(skipFrames + 1), framesToCapture, addrs.data(),
		nullptr);

	trace.frames.reserve(captured);
	alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 512];
	auto* sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
	sym->SizeOfStruct = sizeof(SYMBOL_INFO);
	sym->MaxNameLen = 511;

	IMAGEHLP_LINE64 lineInfo{};
	lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	for (USHORT i = 0; i < captured; ++i) {
		StackFrame out;
		const DWORD64 addr = reinterpret_cast<DWORD64>(addrs[i]);
		out.address = static_cast<uintptr_t>(addr);

		if (symInitialized) {
			DWORD64 displacement = 0;
			if (SymFromAddr(process, addr, &displacement, sym)) {
				out.symbol = sym->Name;
			}
			DWORD lineDisp = 0;
			if (SymGetLineFromAddr64(process, addr, &lineDisp, &lineInfo)) {
				if (lineInfo.FileName) out.file = lineInfo.FileName;
				out.line = static_cast<uint32_t>(lineInfo.LineNumber);
			}
		}
		trace.frames.push_back(std::move(out));
	}

#elif (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__)
	const size_t totalToCapture = maxFrames + skipFrames + 1;
	std::vector<void*> addrs(totalToCapture);
	const int captured =
		::backtrace(addrs.data(), static_cast<int>(totalToCapture));
	if (captured <= 0) return trace;

	char** symbols = ::backtrace_symbols(addrs.data(), captured);
	const size_t start = skipFrames + 1;
	if (static_cast<size_t>(captured) > start) {
		trace.frames.reserve(static_cast<size_t>(captured) - start);
	}

	for (int i = static_cast<int>(start); i < captured; ++i) {
		StackFrame out;
		out.address = reinterpret_cast<uintptr_t>(addrs[i]);

		Dl_info info{};
		if (::dladdr(addrs[i], &info) && info.dli_sname) {
			int status = 0;
			char* demangled =
				abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
			if (status == 0 && demangled) {
				out.symbol = demangled;
			} else {
				out.symbol = info.dli_sname;
			}
			std::free(demangled);
			if (info.dli_fname) {
				out.file = info.dli_fname;
			}
		} else if (symbols && symbols[i]) {
			out.symbol = symbols[i];
		}

		trace.frames.push_back(std::move(out));
	}
	std::free(symbols);

#else
	(void)skipFrames;
	(void)maxFrames;
#endif

	return trace;
}

}  // namespace bird::diag
