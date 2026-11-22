#ifndef BIRD_HOT_RELOAD_MANAGER_H
#define BIRD_HOT_RELOAD_MANAGER_H

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "BuildToolsRunner.h"
#include "FileWatcher.h"
#include "HotReloadTypes.h"
#include "ReloadQueue.h"

namespace bird::hotreload {

/**
 * Top-level coordinator for the Bird Engine asset hot-reload pipeline.
 *
 * Connects the three asynchronous stages:
 * 1. **Background FileWatcher**: Monitors raw source directories with
 *    debouncing (~100 ms) so rapid editor saves coalesce cleanly.
 * 2. **Async BuildToolsRunner**: Invokes `BuildTools` on a worker thread to
 *    recompile only the modified asset into its `.bird` binary while capturing
 *    stdout/stderr. Failed builds are logged and dropped so live assets remain
 *    untouched.
 * 3. **Main-Thread ReloadQueue**: Receives `AssetReloadRequest` messages only
 *    after compilation succeeds. `ResourceManager::pollHotReloads()` drains
 *    this queue once per frame on the main thread and swaps GPU/CPU handles in
 *    place.
 *
 * Also routes modified `.dart` script files directly into the reload queue so
 * registered Dart hot-reload hooks can execute safely on the main thread.
 */
class HotReloadManager {
   public:
	/**
	 * Constructs a HotReloadManager with the provided configuration.
	 *
	 * @param config Watch directories, BuildTools path, and debounce timing.
	 * @param executor Optional custom ProcessExecutor for testing.
	 */
	explicit HotReloadManager(HotReloadConfig config = {},
							  ProcessExecutor executor = nullptr);

	/** Stops the file watcher and build runner cleanly. */
	~HotReloadManager();

	HotReloadManager(const HotReloadManager&) = delete;
	HotReloadManager& operator=(const HotReloadManager&) = delete;

	/**
	 * Starts the background BuildToolsRunner and FileWatcher threads.
	 *
	 * @return True if watching started on at least one configured directory.
	 */
	bool start();

	/**
	 * Stops the FileWatcher and BuildToolsRunner threads and clears pending
	 * reload requests. Safe to call multiple times.
	 */
	void stop();

	/** @return True while the hot-reload pipeline is actively watching. */
	bool isRunning() const { return m_watcher.isRunning(); }

	/**
	 * Drains all completed AssetReloadRequest entries ready for main-thread
	 * handle swapping. Call once per frame from ResourceManager::pollHotReloads().
	 *
	 * @return Batch of reload requests in FIFO order.
	 */
	std::vector<AssetReloadRequest> drainRequests();

	/** @return Direct reference to the thread-safe ReloadQueue. */
	ReloadQueue& getReloadQueue() { return m_reloadQueue; }

	/** @return Current configuration snapshot. */
	const HotReloadConfig& getConfig() const { return m_config; }

	/** @return Total number of assets that compiled successfully since start. */
	uint64_t getSuccessfulBuildCount() const {
		return m_successfulBuilds.load(std::memory_order_relaxed);
	}

	/** @return Total number of failed BuildTools invocations since start. */
	uint64_t getFailedBuildCount() const {
		return m_failedBuilds.load(std::memory_order_relaxed);
	}

	/** @return Error output from the most recent failed build, or empty string. */
	std::string getLastBuildError() const;

	/**
	 * Manually injects a file-change event through the pipeline (bypassing the
	 * OS watcher). Useful for editor "Force Recompile" actions and unit tests.
	 *
	 * @param rawPath Path to the modified source asset.
	 */
	void notifyFileChanged(const std::string& rawPath);

   private:
	void handleDebouncedChange(const std::string& normalizedPath);

	HotReloadConfig m_config;
	FileWatcher m_watcher;
	BuildToolsRunner m_buildRunner;
	ReloadQueue m_reloadQueue;

	std::atomic<uint64_t> m_successfulBuilds{0};
	std::atomic<uint64_t> m_failedBuilds{0};

	mutable std::mutex m_errorMutex;
	std::string m_lastBuildError;
};

}  // namespace bird::hotreload

#endif	// BIRD_HOT_RELOAD_MANAGER_H
