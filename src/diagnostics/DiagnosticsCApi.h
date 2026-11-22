#ifndef BIRD_DIAGNOSTICS_C_API_H
#define BIRD_DIAGNOSTICS_C_API_H

#include <cstdint>

#if defined(_WIN32) || defined(_MSC_VER)
#define BIRD_DIAG_C_API __declspec(dllexport)
#else
#define BIRD_DIAG_C_API __attribute__((visibility("default")))
#endif

/**
 * C ABI surface for the Diagnostics module, consumed by the Dart FFI layer
 * (`bird_diagnostics.dart`) and external tooling.
 */
extern "C" {

/**
 * Emits a structured log message into `bird::diag::Logger`.
 *
 * @param severity Integer matching `bird::diag::LogSeverity` (0=Trace .. 5=Fatal).
 *                 Out-of-range values are clamped to `[0, 5]`.
 * @param category Null-terminated UTF-8 category tag (defaults to `"Dart"` if null).
 * @param message  Null-terminated UTF-8 message string (ignored if null).
 */
BIRD_DIAG_C_API void bird_diag_log(int32_t severity, const char* category,
								   const char* message);

/**
 * Flushes all active sinks on the global Logger.
 */
BIRD_DIAG_C_API void bird_diag_flush(void);

/**
 * Sets the minimum severity threshold on the global Logger.
 *
 * @param severity Integer matching `bird::diag::LogSeverity` (0..5).
 */
BIRD_DIAG_C_API void bird_diag_set_min_severity(int32_t severity);

/**
 * Returns the current minimum severity threshold as an integer (0..5).
 */
BIRD_DIAG_C_API int32_t bird_diag_get_min_severity(void);

}  // extern "C"

#endif	// BIRD_DIAGNOSTICS_C_API_H
