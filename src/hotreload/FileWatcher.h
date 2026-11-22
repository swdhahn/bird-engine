#ifndef BIRD_HOT_RELOAD_FILE_WATCHER_H
#define BIRD_HOT_RELOAD_FILE_WATCHER_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace bird::hotreload {

/**
 * Callback invoked on the FileWatcher background thread once a modified file
 * has settled past the debounce interval.
 *
 * @param normalizedPath Forward-slash normalized path of the changed file.
 */
using FileChangeCallback = std::function<void(const std::string& normalizedPath)>;

/**
 * Cross-platform background directory watcher with event debouncing.
 *
 * Monitors one or more root directories recursively for source asset
 * modifications on a dedicated background thread:
 * - **Linux**: Uses kernel `inotify` with recursive subdirectory watches and a
 *   self-pipe wakeup channel for immediate, non-blocking shutdown.
 * - **Windows**: Uses overlapped `ReadDirectoryChangesW` with an explicit stop
 *   event handle.
 * - **macOS / fallback**: Uses a portable `std::filesystem::last_write_time`
 *   polling loop.
 *
 * Rapid bursts of filesystem events (such as editor truncate + write + rename
 * sequences) are coalesced per path using a configurable debounce window
 * (default 100 ms) so downstream asset compilation is never triggered on
 * partial writes or duplicated.
 *
 * Thread safety: start(), stop(), isRunning(), and setDebounceInterval() are
 * safe to call from the main thread. The registered FileChangeCallback executes
 * on the watcher's background thread.
 */
class FileWatcher {
   public:
	/**
	 * Constructs a FileWatcher without starting the background thread.
	 *
	 * @param debounceInterval Quiet period after the last write event before
	 *        firing the callback. Defaults to 100 ms.
	 * @param pollInterval Maximum wait slice for flushing debounced events and
	 *        polling interval for the portable fallback backend. Defaults to 25 ms.
	 */
	explicit FileWatcher(
		std::chrono::milliseconds debounceInterval = std::chrono::milliseconds(100),
		std::chrono::milliseconds pollInterval = std::chrono::milliseconds(25));

	/** Stops the background thread if running and releases OS watch handles. */
	~FileWatcher();

	FileWatcher(const FileWatcher&) = delete;
	FileWatcher& operator=(const FileWatcher&) = delete;

	/**
	 * Starts watching a single root directory recursively on a background thread.
	 *
	 * @param directory Path to the directory to watch. Must exist and be a directory.
	 * @param callback Function invoked on the background thread for each debounced change.
	 * @return True if watching started successfully; false if the directory does
	 *         not exist, callback is empty, or the watcher is already running.
	 */
	bool start(const std::string& directory, FileChangeCallback callback);

	/**
	 * Starts watching multiple root directories recursively on a background thread.
	 *
	 * Non-existent directories in the list are skipped with a warning; returns
	 * false only if none of the provided directories exist or callback is empty.
	 *
	 * @param directories List of root directories to monitor.
	 * @param callback Function invoked on the background thread for each debounced change.
	 * @return True if at least one directory is being watched.
	 */
	bool start(const std::vector<std::string>& directories,
			   FileChangeCallback callback);

	/**
	 * Signals the background watcher thread to exit and joins it.
	 * Safe to call multiple times or when not running.
	 */
	void stop();

	/** @return True while the background watcher thread is active. */
	bool isRunning() const { return m_running.load(std::memory_order_acquire); }

	/**
	 * Updates the debounce interval used to coalesce rapid file saves.
	 *
	 * @param interval New debounce quiet period.
	 */
	void setDebounceInterval(std::chrono::milliseconds interval);

	/** @return Currently configured debounce interval. */
	std::chrono::milliseconds getDebounceInterval() const;

   private:
	using Clock = std::chrono::steady_clock;

	void recordRawChange(const std::string& rawPath);
	void flushDebouncedEvents();

	void runLoop();
#if defined(__linux__)
	void runInotifyLoop();
#elif defined(_WIN32)
	void runWindowsLoop();
#endif
	void runPollingLoop();

	std::vector<std::string> m_directories;
	FileChangeCallback m_callback;

	mutable std::mutex m_configMutex;
	std::chrono::milliseconds m_debounceInterval;
	std::chrono::milliseconds m_pollInterval;

	std::unordered_map<std::string, Clock::time_point> m_pendingEvents;

	std::atomic<bool> m_running{false};
	std::thread m_thread;

	// Wakeup primitives so stop() returns promptly without waiting for a poll slice.
	std::mutex m_stopMutex;
	std::condition_variable m_stopCv;
#if defined(__linux__)
	int m_wakeupPipe[2]{-1, -1};
#elif defined(_WIN32)
	void* m_stopEvent = nullptr;
#endif
};

}  // namespace bird::hotreload

#endif	// BIRD_HOT_RELOAD_FILE_WATCHER_H
