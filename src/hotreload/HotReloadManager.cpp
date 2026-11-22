#include "HotReloadManager.h"

#include <filesystem>

#include "../platform/Log.h"

namespace fs = std::filesystem;

namespace bird::hotreload {

HotReloadManager::HotReloadManager(HotReloadConfig config,
								   ProcessExecutor executor)
	: m_config(std::move(config)),
	  m_watcher(m_config.debounceInterval, m_config.pollInterval),
	  m_buildRunner(m_config.buildToolsPath, std::move(executor)) {}

HotReloadManager::~HotReloadManager() {
	stop();
}

bool HotReloadManager::start() {
	if (m_watcher.isRunning()) {
		return true;
	}

	std::vector<std::string> dirs;
	if (!m_config.watchDirectory.empty()) {
		dirs.push_back(m_config.watchDirectory);
	}
	if (!m_config.dartWatchDirectory.empty() &&
		m_config.dartWatchDirectory != m_config.watchDirectory) {
		dirs.push_back(m_config.dartWatchDirectory);
	}

	m_buildRunner.start();
	const bool started = m_watcher.start(
		dirs, [this](const std::string& path) { handleDebouncedChange(path); });
	if (!started) {
		m_buildRunner.stop();
		return false;
	}

	platform::log(platform::LogLevel::Info,
				  "[HotReload] Watching source assets in '" +
					  m_config.watchDirectory + "' (debounce " +
					  std::to_string(m_config.debounceInterval.count()) +
					  " ms)");
	return true;
}

void HotReloadManager::stop() {
	m_watcher.stop();
	m_buildRunner.stop();
	m_reloadQueue.clear();
}

std::vector<AssetReloadRequest> HotReloadManager::drainRequests() {
	return m_reloadQueue.drain();
}

std::string HotReloadManager::getLastBuildError() const {
	std::lock_guard<std::mutex> lock(m_errorMutex);
	return m_lastBuildError;
}

void HotReloadManager::notifyFileChanged(const std::string& rawPath) {
	if (isIgnoredHotReloadFile(rawPath)) {
		return;
	}
	const std::string normalized = normalizeAssetPath(rawPath);
	handleDebouncedChange(normalized);
}

void HotReloadManager::handleDebouncedChange(
	const std::string& normalizedPath) {
	const HotReloadAssetKind kind = classifyAssetPath(normalizedPath);
	if (kind == HotReloadAssetKind::Unknown) {
		return;
	}

	// Dart scripts do not go through BuildTools; notify the main thread so
	// the Dart runtime / hot-reload hook can trigger safely.
	if (kind == HotReloadAssetKind::DartScript) {
		AssetReloadRequest req;
		req.assetId = {normalizedPath, HotReloadAssetKind::DartScript};
		m_reloadQueue.push(std::move(req));
		return;
	}

	// Shared GLSL include headers may affect multiple shaders. Rebake the
	// shaders directory incrementally (ShaderBaker skips unchanged shaders).
	if (kind == HotReloadAssetKind::ShaderInclude) {
		std::string shadersDir = m_config.watchDirectory;
		const fs::path candidate = fs::path(m_config.watchDirectory) / "shaders";
		std::error_code ec;
		if (fs::exists(candidate, ec) && fs::is_directory(candidate, ec)) {
			shadersDir = candidate.generic_string();
		}

		m_buildRunner.enqueueDirectoryBuild(
			shadersDir, [this, normalizedPath](const BuildResult& result) {
				if (!result.success) {
					m_failedBuilds.fetch_add(1, std::memory_order_relaxed);
					std::lock_guard<std::mutex> lock(m_errorMutex);
					m_lastBuildError = result.stderrOutput.empty()
										   ? result.stdoutOutput
										   : result.stderrOutput;
					return;
				}
				m_successfulBuilds.fetch_add(1, std::memory_order_relaxed);
				AssetReloadRequest req;
				req.assetId = {normalizedPath,
							   HotReloadAssetKind::ShaderInclude};
				m_reloadQueue.push(std::move(req));
			});
		return;
	}

	// Single-asset recompile via BuildTools --file <path> --force.
	m_buildRunner.enqueueFileBuild(
		normalizedPath,
		[this, normalizedPath, kind](const BuildResult& result) {
			if (!result.success) {
				m_failedBuilds.fetch_add(1, std::memory_order_relaxed);
				std::lock_guard<std::mutex> lock(m_errorMutex);
				m_lastBuildError = result.stderrOutput.empty()
									   ? result.stdoutOutput
									   : result.stderrOutput;
				return;
			}
			m_successfulBuilds.fetch_add(1, std::memory_order_relaxed);
			AssetReloadRequest req;
			req.assetId = {normalizedPath, kind};
			req.compiledPath = normalizedPath + ".bird";
			m_reloadQueue.push(std::move(req));
		});
}

}  // namespace bird::hotreload
