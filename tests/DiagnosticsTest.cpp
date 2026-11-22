#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "diagnostics/CrashHandler.h"
#include "diagnostics/DiagnosticsCApi.h"
#include "diagnostics/FallbackAssets.h"
#include "diagnostics/LogSinks.h"
#include "diagnostics/Logger.h"
#include "diagnostics/Profiler.h"
#include "diagnostics/StackTrace.h"
#include "platform/Log.h"

namespace fs = std::filesystem;
using namespace bird::diag;

namespace {

fs::path makeTempDir(const std::string& suffix) {
	const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
	fs::path dir = fs::temp_directory_path() /
				   ("bird_diag_test_" + suffix + "_" + std::to_string(now));
	fs::create_directories(dir);
	return dir;
}

std::string readWholeFile(const fs::path& p) {
	std::ifstream in(p);
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

}  // namespace

// ============================================================================
// Structured Logger & Sinks
// ============================================================================

TEST(DiagnosticsLogger, SeverityLabelsAndFormatting) {
	EXPECT_STREQ(severityToString(LogSeverity::Trace), "TRACE");
	EXPECT_STREQ(severityToString(LogSeverity::Debug), "DEBUG");
	EXPECT_STREQ(severityToString(LogSeverity::Info), "INFO");
	EXPECT_STREQ(severityToString(LogSeverity::Warn), "WARN");
	EXPECT_STREQ(severityToString(LogSeverity::Error), "ERROR");
	EXPECT_STREQ(severityToString(LogSeverity::Fatal), "FATAL");

	LogRecord rec;
	rec.epochMillis = 0;  // 1970-01-01 00:00:00.000 UTC
	rec.severity = LogSeverity::Warn;
	rec.category = "Physics";
	rec.message = "Body sleep threshold reached";

	EXPECT_EQ(rec.formatTimestamp(), "1970-01-01 00:00:00.000");
	EXPECT_EQ(
		rec.format(),
		"[1970-01-01 00:00:00.000] [WARN] [Physics] Body sleep threshold reached");
}

TEST(DiagnosticsLogger, RingBufferWrapsAndFiltersCorrectly) {
	RingBufferLogSink ring(4);
	for (uint64_t i = 1; i <= 6; ++i) {
		LogRecord rec;
		rec.sequence = i;
		rec.severity = (i % 2 == 0) ? LogSeverity::Error : LogSeverity::Info;
		rec.category = (i <= 4) ? "Renderer" : "Dart";
		rec.message = "msg_" + std::to_string(i);
		ring.write(rec);
	}

	EXPECT_EQ(ring.size(), 4u);
	EXPECT_EQ(ring.capacity(), 4u);
	EXPECT_EQ(ring.latestSequence(), 6u);

	const auto all = ring.snapshot();
	ASSERT_EQ(all.size(), 4u);
	EXPECT_EQ(all[0].sequence, 3u);
	EXPECT_EQ(all[3].sequence, 6u);

	const auto last2 = ring.snapshot(2);
	ASSERT_EQ(last2.size(), 2u);
	EXPECT_EQ(last2[0].sequence, 5u);
	EXPECT_EQ(last2[1].sequence, 6u);

	const auto since4 = ring.snapshotSince(4);
	ASSERT_EQ(since4.size(), 2u);
	EXPECT_EQ(since4[0].sequence, 5u);
	EXPECT_EQ(since4[1].sequence, 6u);

	const auto errorsOnly = ring.snapshotFiltered(LogSeverity::Error);
	ASSERT_EQ(errorsOnly.size(), 2u);
	EXPECT_EQ(errorsOnly[0].sequence, 4u);
	EXPECT_EQ(errorsOnly[1].sequence, 6u);

	const auto dartOnly = ring.snapshotFiltered(LogSeverity::Trace, "Dart");
	ASSERT_EQ(dartOnly.size(), 2u);
	EXPECT_EQ(dartOnly[0].sequence, 5u);
	EXPECT_EQ(dartOnly[1].sequence, 6u);

	ring.clear();
	EXPECT_EQ(ring.size(), 0u);
	EXPECT_EQ(ring.latestSequence(), 0u);
}

TEST(DiagnosticsLogger, RotatingFileSinkRotatesAndFlushesOnError) {
	const fs::path tempDir = makeTempDir("rotate");
	const fs::path logPath = tempDir / "engine.log";

	{
		RotatingFileLogSink sink(logPath.string(), /*maxFileSizeBytes=*/120,
								 /*maxFiles=*/2);
		ASSERT_TRUE(sink.isOpen());

		for (int i = 0; i < 10; ++i) {
			LogRecord rec;
			rec.epochMillis = 1700000000000LL + i * 1000;
			rec.severity = (i == 9) ? LogSeverity::Error : LogSeverity::Info;
			rec.category = "Test";
			rec.message = "Payload line number " + std::to_string(i);
			sink.write(rec);
		}
		// Error on i==9 must have auto-flushed even before sink destruction.
		const std::string activeContent = readWholeFile(logPath);
		EXPECT_NE(activeContent.find("Payload line number 9"),
				  std::string::npos);
	}

	EXPECT_TRUE(fs::exists(logPath));
	EXPECT_TRUE(fs::exists(tempDir / "engine.1.log"));
	EXPECT_TRUE(fs::exists(tempDir / "engine.2.log"));
	EXPECT_FALSE(fs::exists(tempDir / "engine.3.log"));

	std::error_code ec;
	fs::remove_all(tempDir, ec);
}

TEST(DiagnosticsLogger, MultiThreadedLoggingAndPlatformBridge) {
	LoggerConfig cfg;
	cfg.minSeverity = LogSeverity::Debug;
	cfg.enableConsoleSink = false;
	cfg.enableRingBufferSink = true;
	cfg.ringBufferCapacity = 512;
	cfg.bridgePlatformLog = true;

	Logger::get().init(cfg);

	// Trace should be filtered out at minSeverity=Debug.
	BIRD_LOG_TRACE("Filter", "dropped");
	EXPECT_EQ(Logger::get().getRingBufferSink()->size(), 0u);

	// Platform bridge should forward into Logger under "Platform".
	bird::platform::log(bird::platform::LogLevel::Warning, "legacy warning");
	auto snap = Logger::get().getRingBufferSink()->snapshot();
	ASSERT_EQ(snap.size(), 1u);
	EXPECT_EQ(snap[0].severity, LogSeverity::Warn);
	EXPECT_EQ(snap[0].category, "Platform");
	EXPECT_EQ(snap[0].message, "legacy warning");

	Logger::get().getRingBufferSink()->clear();

	// 8 threads x 40 messages = 320 messages (< 512 ring capacity).
	constexpr int kThreads = 8;
	constexpr int kPerThread = 40;
	std::vector<std::thread> workers;
	workers.reserve(kThreads);
	for (int t = 0; t < kThreads; ++t) {
		workers.emplace_back([t]() {
			for (int i = 0; i < kPerThread; ++i) {
				BIRD_LOG_INFO("Worker", "t=" + std::to_string(t) + " i=" +
											std::to_string(i));
			}
		});
	}
	for (auto& w : workers) w.join();

	const auto records = Logger::get().getRingBufferSink()->snapshot();
	EXPECT_EQ(records.size(), static_cast<size_t>(kThreads * kPerThread));

	// Verify C API round-trip.
	bird_diag_set_min_severity(static_cast<int32_t>(LogSeverity::Warn));
	EXPECT_EQ(bird_diag_get_min_severity(),
			  static_cast<int32_t>(LogSeverity::Warn));
	bird_diag_log(static_cast<int32_t>(LogSeverity::Info), "Dart",
				  "should be filtered");
	bird_diag_log(static_cast<int32_t>(LogSeverity::Error), "Dart",
				  "dart error message");
	bird_diag_flush();

	const auto dartErrors =
		Logger::get().getRingBufferSink()->snapshotFiltered(LogSeverity::Error,
															"Dart");
	ASSERT_EQ(dartErrors.size(), 1u);
	EXPECT_EQ(dartErrors[0].message, "dart error message");

	Logger::get().shutdown();
}

// ============================================================================
// Profiler
// ============================================================================

TEST(DiagnosticsProfiler, RecordsFramesSectionsAndSummary) {
	auto& prof = Profiler::get();
	prof.reset();
	prof.setEnabled(true);
	prof.setHistoryCapacity(16);

	// First markFrame starts the clock.
	PROFILE_FRAME();

	for (int f = 0; f < 5; ++f) {
		{
			PROFILE_SCOPE("Update");
			{
				PROFILE_SCOPE("Physics");
				std::this_thread::sleep_for(std::chrono::microseconds(200));
			}
			{
				PROFILE_SCOPE("Physics");
				std::this_thread::sleep_for(std::chrono::microseconds(200));
			}
		}
		{
			PROFILE_SCOPE("Render");
			std::this_thread::sleep_for(std::chrono::microseconds(300));
		}
		PROFILE_FRAME();
	}

	const auto hist = prof.history();
	ASSERT_EQ(hist.size(), 5u);
	EXPECT_EQ(hist.front().frameIndex, 1u);
	EXPECT_EQ(hist.back().frameIndex, 5u);

	const auto latest = prof.latestFrame();
	EXPECT_GT(latest.frameTimeMs, 0.0);
	EXPECT_GT(latest.fps, 0.0);
	ASSERT_EQ(latest.sections.size(), 3u);

	// Physics exited before Update, and was invoked twice at depth 1.
	bool foundPhysics = false;
	bool foundUpdate = false;
	bool foundRender = false;
	for (const auto& s : latest.sections) {
		if (s.name == "Physics") {
			foundPhysics = true;
			EXPECT_EQ(s.callCount, 2u);
			EXPECT_EQ(s.depth, 1u);
			EXPECT_GT(s.durationMs, 0.0);
		} else if (s.name == "Update") {
			foundUpdate = true;
			EXPECT_EQ(s.callCount, 1u);
			EXPECT_EQ(s.depth, 0u);
		} else if (s.name == "Render") {
			foundRender = true;
			EXPECT_EQ(s.callCount, 1u);
			EXPECT_EQ(s.depth, 0u);
		}
	}
	EXPECT_TRUE(foundPhysics);
	EXPECT_TRUE(foundUpdate);
	EXPECT_TRUE(foundRender);

	const auto sum = prof.summary(5);
	EXPECT_EQ(sum.sampleCount, 5u);
	EXPECT_GT(sum.averageFps, 0.0);
	EXPECT_LE(sum.minFrameTimeMs, sum.averageFrameTimeMs);
	EXPECT_LE(sum.averageFrameTimeMs, sum.maxFrameTimeMs);
	EXPECT_LE(sum.p95FrameTimeMs, sum.maxFrameTimeMs);

	EXPECT_GT(bird_diag_get_average_fps(), 0.0);
	EXPECT_GT(bird_diag_get_last_frame_ms(), 0.0);

	// Ring buffer wrap-around check.
	prof.setHistoryCapacity(3);
	prof.reset();
	PROFILE_FRAME();
	for (int i = 0; i < 6; ++i) {
		PROFILE_FRAME();
	}
	const auto wrapped = prof.history();
	ASSERT_EQ(wrapped.size(), 3u);
	EXPECT_EQ(wrapped[0].frameIndex, 4u);
	EXPECT_EQ(wrapped[2].frameIndex, 6u);

	// Runtime disable suppresses new frames.
	prof.setEnabled(false);
	PROFILE_FRAME();
	EXPECT_EQ(prof.latestFrame().frameIndex, 6u);
	prof.setEnabled(true);
	prof.reset();
}

// ============================================================================
// Stack Trace & Crash Handler
// ============================================================================

TEST(DiagnosticsCrash, CapturesStackTraceAndWritesCrashReport) {
	const StackTrace trace = captureStackTrace(0, 32);
	const std::string formatted = trace.format();
	EXPECT_FALSE(formatted.empty());
#if (defined(__linux__) && !defined(__ANDROID__)) || defined(__APPLE__) || \
	defined(_WIN32)
	EXPECT_FALSE(trace.empty());
#endif

	const fs::path tempDir = makeTempDir("crash");
	const fs::path logPath = tempDir / "session.log";

	LoggerConfig logCfg;
	logCfg.enableConsoleSink = false;
	logCfg.logFilePath = logPath.string();
	Logger::get().init(logCfg);
	BIRD_LOG_INFO("Engine", "Before simulated crash");

	std::string callbackPath;
	CrashHandlerConfig crashCfg;
	crashCfg.dumpDirectory = (tempDir / "dumps").string();
	crashCfg.dumpFilePrefix = "test_crash";
	crashCfg.flushLogger = true;
	crashCfg.onCrashCallback = [&](const std::string& p) { callbackPath = p; };

	CrashHandler::install(crashCfg);
	EXPECT_TRUE(CrashHandler::isInstalled());

	const std::string reportPath =
		CrashHandler::writeCrashReport("Simulated fatal fault", trace);
	ASSERT_FALSE(reportPath.empty());
	EXPECT_EQ(callbackPath, reportPath);
	EXPECT_TRUE(fs::exists(reportPath));

	const std::string reportText = readWholeFile(reportPath);
	EXPECT_NE(reportText.find("Simulated fatal fault"), std::string::npos);
	EXPECT_NE(reportText.find("Stack Trace:"), std::string::npos);
	EXPECT_NE(reportText.find(logPath.string()), std::string::npos);

	// Logger should have been flushed so our Info line is already on disk.
	const std::string logText = readWholeFile(logPath);
	EXPECT_NE(logText.find("Before simulated crash"), std::string::npos);

	CrashHandler::uninstall();
	EXPECT_FALSE(CrashHandler::isInstalled());
	Logger::get().shutdown();

	std::error_code ec;
	fs::remove_all(tempDir, ec);
}

// ============================================================================
// Fallback Assets (CPU Data)
// ============================================================================

TEST(DiagnosticsFallbacks, CheckerboardTextureIsMagentaAndBlack) {
	const FallbackTextureData tex = generateCheckerboardTextureData(16, 4);
	EXPECT_EQ(tex.width, 16u);
	EXPECT_EQ(tex.height, 16u);
	EXPECT_EQ(tex.channels, 4u);
	ASSERT_EQ(tex.pixels.size(), 16u * 16u * 4u);

	// Pixel (0, 0): cell (0, 0) -> magenta (255, 0, 255, 255)
	EXPECT_EQ(tex.pixels[0], 0xFF);
	EXPECT_EQ(tex.pixels[1], 0x00);
	EXPECT_EQ(tex.pixels[2], 0xFF);
	EXPECT_EQ(tex.pixels[3], 0xFF);

	// Pixel (4, 0): cell (1, 0) -> black (0, 0, 0, 255)
	const size_t p4 = 4u * 4u;
	EXPECT_EQ(tex.pixels[p4 + 0], 0x00);
	EXPECT_EQ(tex.pixels[p4 + 1], 0x00);
	EXPECT_EQ(tex.pixels[p4 + 2], 0x00);
	EXPECT_EQ(tex.pixels[p4 + 3], 0xFF);

	// Pixel (4, 4): cell (1, 1) -> magenta again
	const size_t p44 = (4u * 16u + 4u) * 4u;
	EXPECT_EQ(tex.pixels[p44 + 0], 0xFF);
	EXPECT_EQ(tex.pixels[p44 + 1], 0x00);
	EXPECT_EQ(tex.pixels[p44 + 2], 0xFF);
	EXPECT_EQ(tex.pixels[p44 + 3], 0xFF);
}

TEST(DiagnosticsFallbacks, UnitCubeGeometryIsValid) {
	const FallbackMeshData cube = generateUnitCubeMeshData();
	EXPECT_EQ(cube.positions.size(), 24u * 3u);
	EXPECT_EQ(cube.normals.size(), 24u * 3u);
	EXPECT_EQ(cube.texCoords.size(), 24u * 2u);
	EXPECT_EQ(cube.indices.size(), 36u);

	for (size_t i = 0; i < 24; ++i) {
		const float px = cube.positions[i * 3 + 0];
		const float py = cube.positions[i * 3 + 1];
		const float pz = cube.positions[i * 3 + 2];
		EXPECT_NEAR(std::abs(px), 0.5f, 1e-5f);
		EXPECT_NEAR(std::abs(py), 0.5f, 1e-5f);
		EXPECT_NEAR(std::abs(pz), 0.5f, 1e-5f);

		const float nx = cube.normals[i * 3 + 0];
		const float ny = cube.normals[i * 3 + 1];
		const float nz = cube.normals[i * 3 + 2];
		const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
		EXPECT_NEAR(len, 1.0f, 1e-5f);

		const float u = cube.texCoords[i * 2 + 0];
		const float v = cube.texCoords[i * 2 + 1];
		EXPECT_GE(u, 0.0f);
		EXPECT_LE(u, 1.0f);
		EXPECT_GE(v, 0.0f);
		EXPECT_LE(v, 1.0f);
	}

	for (uint32_t idx : cube.indices) {
		EXPECT_LT(idx, 24u);
	}
}
