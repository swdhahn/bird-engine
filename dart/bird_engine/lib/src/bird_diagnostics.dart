import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

import 'generated/bindings.dart' as b;

/// Severity levels matching C++ `bird::diag::LogSeverity`.
enum LogSeverity {
  trace(0),
  debug(1),
  info(2),
  warn(3),
  error(4),
  fatal(5);

  const LogSeverity(this.value);

  /// Wire integer passed across the C ABI.
  final int value;

  /// Converts a raw integer from the C ABI back to [LogSeverity].
  static LogSeverity fromValue(int value) {
    final clamped = value.clamp(0, LogSeverity.values.length - 1);
    return LogSeverity.values[clamped];
  }
}

typedef _DiagLogNative = ffi.Void Function(
  ffi.Int32 severity,
  ffi.Pointer<Utf8> category,
  ffi.Pointer<Utf8> message,
);
typedef _DiagLogDart = void Function(
  int severity,
  ffi.Pointer<Utf8> category,
  ffi.Pointer<Utf8> message,
);

typedef _DiagFlushNative = ffi.Void Function();
typedef _DiagFlushDart = void Function();

typedef _DiagSetMinSeverityNative = ffi.Void Function(ffi.Int32 severity);
typedef _DiagSetMinSeverityDart = void Function(int severity);

typedef _DiagGetMinSeverityNative = ffi.Int32 Function();
typedef _DiagGetMinSeverityDart = int Function();

late final _DiagLogDart _diagLog = b.nativeLib
    .lookupFunction<_DiagLogNative, _DiagLogDart>('bird_diag_log');

late final _DiagFlushDart _diagFlush = b.nativeLib
    .lookupFunction<_DiagFlushNative, _DiagFlushDart>('bird_diag_flush');

late final _DiagSetMinSeverityDart _diagSetMinSeverity = b.nativeLib
    .lookupFunction<_DiagSetMinSeverityNative, _DiagSetMinSeverityDart>(
        'bird_diag_set_min_severity');

late final _DiagGetMinSeverityDart _diagGetMinSeverity = b.nativeLib
    .lookupFunction<_DiagGetMinSeverityNative, _DiagGetMinSeverityDart>(
        'bird_diag_get_min_severity');

/// Structured logging bridge into the native `bird::diag::Logger`.
///
/// Messages emitted through [BirdLog] flow into all active native sinks
/// (console, rotating `.log` file, and the in-memory ring buffer used by
/// the in-game log console UI).
abstract final class BirdLog {
  /// Emits a structured log message with the given [severity] and [category].
  static void log(LogSeverity severity, String category, String message) {
    final catPtr = category.toNativeUtf8();
    final msgPtr = message.toNativeUtf8();
    try {
      _diagLog(severity.value, catPtr, msgPtr);
    } finally {
      calloc.free(catPtr);
      calloc.free(msgPtr);
    }
  }

  /// Emits a `TRACE`-level log message.
  static void trace(String category, String message) =>
      log(LogSeverity.trace, category, message);

  /// Emits a `DEBUG`-level log message.
  static void debug(String category, String message) =>
      log(LogSeverity.debug, category, message);

  /// Emits an `INFO`-level log message.
  static void info(String category, String message) =>
      log(LogSeverity.info, category, message);

  /// Emits a `WARN`-level log message.
  static void warn(String category, String message) =>
      log(LogSeverity.warn, category, message);

  /// Emits an `ERROR`-level log message (auto-flushes sinks).
  static void error(String category, String message) =>
      log(LogSeverity.error, category, message);

  /// Emits a `FATAL`-level log message (auto-flushes sinks).
  static void fatal(String category, String message) =>
      log(LogSeverity.fatal, category, message);

  /// Flushes all active native log sinks immediately.
  static void flush() => _diagFlush();

  /// Updates the minimum severity threshold accepted by the native logger.
  static void setMinSeverity(LogSeverity severity) =>
      _diagSetMinSeverity(severity.value);

  /// Returns the current minimum severity threshold on the native logger.
  static LogSeverity getMinSeverity() =>
      LogSeverity.fromValue(_diagGetMinSeverity());
}
