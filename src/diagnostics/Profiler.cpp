#include "Profiler.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace bird::diag {

namespace {

thread_local uint32_t g_scopeDepth = 0;

}  // namespace

Profiler& Profiler::get() {
	static Profiler instance;
	return instance;
}

Profiler::Profiler() : m_history(kDefaultHistoryCapacity) {}

void Profiler::markFrame() {
	if (!m_enabled.load(std::memory_order_relaxed)) return;

	const auto now = Clock::now();
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_hasFrameStart) {
		m_frameStart = now;
		m_hasFrameStart = true;
		return;
	}

	const double frameTimeMs =
		std::chrono::duration<double, std::milli>(now - m_frameStart).count();
	m_frameStart = now;
	++m_frameIndex;

	FrameProfileSample sample;
	sample.frameIndex = m_frameIndex;
	sample.frameTimeMs = frameTimeMs;
	sample.fps = (frameTimeMs > 0.0) ? (1000.0 / frameTimeMs) : 0.0;
	sample.sections = std::move(m_activeSections);
	m_activeSections.clear();

	m_history[m_head] = std::move(sample);
	m_head = (m_head + 1) % m_capacity;
	if (m_count < m_capacity) {
		++m_count;
	}
}

void Profiler::recordSection(const char* name, double durationMs,
							 uint32_t depth) {
	if (!name || !m_enabled.load(std::memory_order_relaxed)) return;

	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& existing : m_activeSections) {
		if (existing.depth == depth && existing.name == name) {
			existing.durationMs += durationMs;
			++existing.callCount;
			return;
		}
	}
	ProfileSectionSample entry;
	entry.name = name;
	entry.durationMs = durationMs;
	entry.callCount = 1;
	entry.depth = depth;
	m_activeSections.push_back(std::move(entry));
}

void Profiler::setEnabled(bool enabled) {
	m_enabled.store(enabled, std::memory_order_relaxed);
}

bool Profiler::isEnabled() const {
	return m_enabled.load(std::memory_order_relaxed);
}

void Profiler::setHistoryCapacity(size_t capacity) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_capacity = std::max<size_t>(1, capacity);
	m_history.assign(m_capacity, FrameProfileSample{});
	m_head = 0;
	m_count = 0;
}

size_t Profiler::historyCapacity() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_capacity;
}

FrameProfileSample Profiler::latestFrame() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return FrameProfileSample{};
	const size_t newest = (m_head + m_capacity - 1) % m_capacity;
	return m_history[newest];
}

std::vector<FrameProfileSample> Profiler::history(size_t maxFrames) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_count == 0) return {};

	const size_t resultCount =
		(maxFrames == 0 || maxFrames >= m_count) ? m_count : maxFrames;
	std::vector<FrameProfileSample> out;
	out.reserve(resultCount);

	const size_t oldest = (m_head + m_capacity - m_count) % m_capacity;
	const size_t skip = m_count - resultCount;
	for (size_t i = 0; i < resultCount; ++i) {
		out.push_back(m_history[(oldest + skip + i) % m_capacity]);
	}
	return out;
}

ProfilerSummary Profiler::summary(size_t windowFrames) const {
	const auto frames = history(windowFrames);
	ProfilerSummary out;
	if (frames.empty()) return out;

	out.sampleCount = frames.size();
	std::vector<double> durations;
	durations.reserve(frames.size());
	double totalMs = 0.0;
	for (const auto& f : frames) {
		durations.push_back(f.frameTimeMs);
		totalMs += f.frameTimeMs;
	}
	std::sort(durations.begin(), durations.end());

	out.minFrameTimeMs = durations.front();
	out.maxFrameTimeMs = durations.back();
	out.averageFrameTimeMs = totalMs / static_cast<double>(durations.size());
	out.averageFps =
		(out.averageFrameTimeMs > 0.0) ? (1000.0 / out.averageFrameTimeMs) : 0.0;

	const size_t p95Index = static_cast<size_t>(
		std::ceil(0.95 * static_cast<double>(durations.size()))) - 1;
	out.p95FrameTimeMs =
		durations[std::min(p95Index, durations.size() - 1)];
	return out;
}

void Profiler::reset() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_head = 0;
	m_count = 0;
	m_hasFrameStart = false;
	m_frameIndex = 0;
	m_activeSections.clear();
}

// ============================================================================
// ProfileScope
// ============================================================================

ProfileScope::ProfileScope(const char* name) : m_name(name) {
	if (m_name && Profiler::get().isEnabled()) {
		m_active = true;
		m_depth = g_scopeDepth++;
		m_start = Clock::now();
	}
}

ProfileScope::~ProfileScope() {
	if (!m_active) return;
	const auto end = Clock::now();
	if (g_scopeDepth > 0) --g_scopeDepth;
	const double durationMs =
		std::chrono::duration<double, std::milli>(end - m_start).count();
	Profiler::get().recordSection(m_name, durationMs, m_depth);
}

}  // namespace bird::diag
