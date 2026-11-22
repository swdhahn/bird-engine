#ifndef BIRD_DIAGNOSTICS_PROFILER_H
#define BIRD_DIAGNOSTICS_PROFILER_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifndef BIRD_ENABLE_PROFILING
#define BIRD_ENABLE_PROFILING 1
#endif

#if BIRD_ENABLE_PROFILING && defined(BIRD_ENABLE_TRACY)
#include <tracy/Tracy.hpp>
#endif

namespace bird::diag {

/**
 * Aggregated timing sample for a single named CPU section within one frame.
 *
 * Repeated invocations of the same `(name, depth)` within a single frame are
 * combined into one entry with summed `durationMs` and incremented `callCount`.
 */
struct ProfileSectionSample {
	/** Name passed to `PROFILE_SCOPE(name)`. */
	std::string name;
	/** Total accumulated CPU time for this section during the frame, in ms. */
	double durationMs = 0.0;
	/** Number of times this section executed during the frame. */
	uint32_t callCount = 0;
	/** Nesting depth (0 = top-level scope in the frame). */
	uint32_t depth = 0;
};

/**
 * Snapshot of total frame duration and per-section CPU breakdown for a single
 * rendered/ticked frame.
 */
struct FrameProfileSample {
	/** 1-based monotonic frame counter. */
	uint64_t frameIndex = 0;
	/** Total elapsed wall time for the frame in milliseconds. */
	double frameTimeMs = 0.0;
	/** Instantaneous frames per second (`1000.0 / frameTimeMs`, or 0.0). */
	double fps = 0.0;
	/** Ordered list of CPU section timings captured during this frame. */
	std::vector<ProfileSectionSample> sections;
};

/**
 * Rolling summary statistics computed over the most recent frames in the
 * profiler ring buffer. Designed for lightweight in-game FPS / frame-time
 * UI overlays.
 */
struct ProfilerSummary {
	/** Mean FPS over the sampled window (`1000.0 / averageFrameTimeMs`). */
	double averageFps = 0.0;
	/** Arithmetic mean frame time in milliseconds. */
	double averageFrameTimeMs = 0.0;
	/** Fastest frame time in the window in milliseconds. */
	double minFrameTimeMs = 0.0;
	/** Slowest frame time in the window in milliseconds. */
	double maxFrameTimeMs = 0.0;
	/** 95th-percentile frame time in milliseconds. */
	double p95FrameTimeMs = 0.0;
	/** Number of frames included in the summary. */
	size_t sampleCount = 0;
};

/**
 * Thread-safe internal CPU profiler with a fixed-capacity frame-time and
 * section-breakdown ring buffer.
 *
 * Works standalone for in-engine UI overlays and pairs seamlessly with Tracy
 * when compiled with `BIRD_ENABLE_TRACY`.
 *
 * Usage:
 * @code
 * void Application::tick() {
 *     PROFILE_FRAME();
 *     {
 *         PROFILE_SCOPE("Physics");
 *         stepPhysics();
 *     }
 *     {
 *         PROFILE_SCOPE("Render");
 *         renderScene();
 *     }
 * }
 * @endcode
 */
class Profiler {
   public:
	/** Default number of frames kept in the history ring buffer. */
	static constexpr size_t kDefaultHistoryCapacity = 240;

	/** Returns the process-wide Profiler singleton. */
	static Profiler& get();

	Profiler(const Profiler&) = delete;
	Profiler& operator=(const Profiler&) = delete;

	/**
	 * Marks a frame boundary: commits the elapsed time and accumulated
	 * CPU sections since the previous `markFrame()` call into the ring buffer,
	 * then begins timing the next frame.
	 *
	 * On the very first call after construction or `reset()`, starts the frame
	 * clock without pushing a zero-duration frame.
	 */
	void markFrame();

	/**
	 * Records a completed CPU section duration into the active frame.
	 * Called automatically by `ProfileScope` on scope exit.
	 *
	 * @param name Static or null-terminated section name.
	 * @param durationMs Elapsed section duration in milliseconds.
	 * @param depth Nesting depth of the scope (0 = outermost).
	 */
	void recordSection(const char* name, double durationMs,
					   uint32_t depth = 0);

	/**
	 * Enables or disables internal sample collection at runtime.
	 * Does not affect compile-time macro elimination.
	 */
	void setEnabled(bool enabled);

	/** Returns true if internal sample collection is currently enabled. */
	bool isEnabled() const;

	/**
	 * Resizes the frame history ring buffer and clears existing samples.
	 *
	 * @param capacity New maximum frame count (clamped to >= 1).
	 */
	void setHistoryCapacity(size_t capacity);

	/** Returns the maximum number of frames retained in the ring buffer. */
	size_t historyCapacity() const;

	/**
	 * Returns the most recently completed frame sample, or a zero-initialized
	 * `FrameProfileSample` if no frame has completed yet.
	 */
	FrameProfileSample latestFrame() const;

	/**
	 * Returns a chronological copy of completed frame samples.
	 *
	 * @param maxFrames Maximum number of most-recent frames to return (0 = all).
	 * @return Vector of `FrameProfileSample` ordered oldest to newest.
	 */
	std::vector<FrameProfileSample> history(size_t maxFrames = 0) const;

	/**
	 * Computes rolling FPS and frame-time statistics over the last
	 * `windowFrames` completed frames.
	 *
	 * @param windowFrames Number of most-recent frames to aggregate (0 = all).
	 * @return Aggregated `ProfilerSummary`.
	 */
	ProfilerSummary summary(size_t windowFrames = 60) const;

	/** Clears all buffered frame history, active sections, and frame clock. */
	void reset();

   private:
	Profiler();
	~Profiler() = default;

	using Clock = std::chrono::steady_clock;

	mutable std::mutex m_mutex;
	std::atomic<bool> m_enabled{true};

	std::vector<FrameProfileSample> m_history;
	size_t m_capacity = kDefaultHistoryCapacity;
	size_t m_head = 0;
	size_t m_count = 0;

	bool m_hasFrameStart = false;
	Clock::time_point m_frameStart{};
	uint64_t m_frameIndex = 0;
	std::vector<ProfileSectionSample> m_activeSections;
};

/**
 * RAII timer that measures a lexical C++ scope and reports its elapsed CPU
 * time to `Profiler::get()` on destruction.
 */
class ProfileScope {
   public:
	/**
	 * Begins timing the named scope on the current thread.
	 *
	 * @param name Null-terminated section name (must outlive the scope, e.g.
	 *             a string literal).
	 */
	explicit ProfileScope(const char* name);
	~ProfileScope();

	ProfileScope(const ProfileScope&) = delete;
	ProfileScope& operator=(const ProfileScope&) = delete;

   private:
	using Clock = std::chrono::steady_clock;

	const char* m_name;
	uint32_t m_depth = 0;
	bool m_active = false;
	Clock::time_point m_start{};
};

}  // namespace bird::diag

// ============================================================================
// Profiling Macros
// ============================================================================

#define BIRD_DIAG_CONCAT_IMPL(a, b) a##b
#define BIRD_DIAG_CONCAT(a, b) BIRD_DIAG_CONCAT_IMPL(a, b)

#if BIRD_ENABLE_PROFILING
#if defined(BIRD_ENABLE_TRACY)
#define PROFILE_FRAME()                         \
	do {                                        \
		FrameMark;                              \
		::bird::diag::Profiler::get().markFrame(); \
	} while (0)
#define PROFILE_SCOPE(name)                                           \
	ZoneScopedN(name);                                                \
	::bird::diag::ProfileScope BIRD_DIAG_CONCAT(_birdProfScope_,      \
												__LINE__)(name)
#else
#define PROFILE_FRAME() ::bird::diag::Profiler::get().markFrame()
#define PROFILE_SCOPE(name)                                           \
	::bird::diag::ProfileScope BIRD_DIAG_CONCAT(_birdProfScope_,      \
												__LINE__)(name)
#endif
#define PROFILE_FUNCTION() PROFILE_SCOPE(__func__)
#else
#define PROFILE_FRAME() ((void)0)
#define PROFILE_SCOPE(name) ((void)0)
#define PROFILE_FUNCTION() ((void)0)
#endif

#endif	// BIRD_DIAGNOSTICS_PROFILER_H
