#include "DiagnosticsCApi.h"

#include <algorithm>

#include "Logger.h"

namespace {

bird::diag::LogSeverity clampSeverity(int32_t raw) {
	const int32_t clamped = std::clamp(raw, 0, 5);
	return static_cast<bird::diag::LogSeverity>(clamped);
}

}  // namespace

extern "C" {

BIRD_DIAG_C_API void bird_diag_log(int32_t severity, const char* category,
								   const char* message) {
	if (!message) return;
	const char* cat = (category && category[0] != '\0') ? category : "Dart";
	bird::diag::Logger::get().log(clampSeverity(severity), cat, message);
}

BIRD_DIAG_C_API void bird_diag_flush(void) {
	bird::diag::Logger::get().flush();
}

BIRD_DIAG_C_API void bird_diag_set_min_severity(int32_t severity) {
	bird::diag::Logger::get().setMinSeverity(clampSeverity(severity));
}

BIRD_DIAG_C_API int32_t bird_diag_get_min_severity(void) {
	return static_cast<int32_t>(bird::diag::Logger::get().getMinSeverity());
}

}  // extern "C"
