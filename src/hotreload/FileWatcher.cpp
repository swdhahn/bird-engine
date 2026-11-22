#include "FileWatcher.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "../platform/Log.h"
#include "HotReloadTypes.h"

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace bird::hotreload {

FileWatcher::FileWatcher(std::chrono::milliseconds debounceInterval,
						 std::chrono::milliseconds pollInterval)
	: m_debounceInterval(debounceInterval), m_pollInterval(pollInterval) {}

FileWatcher::~FileWatcher() {
	stop();
}

bool FileWatcher::start(const std::string& directory,
						FileChangeCallback callback) {
	return start(std::vector<std::string>{directory}, std::move(callback));
}

bool FileWatcher::start(const std::vector<std::string>& directories,
						FileChangeCallback callback) {
	if (isRunning() || !callback) {
		return false;
	}

	std::vector<std::string> validDirs;
	for (const std::string& dir : directories) {
		if (dir.empty()) continue;
		std::error_code ec;
		if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
			validDirs.push_back(dir);
		} else {
			platform::log(platform::LogLevel::Warning,
						  "[HotReload] Watch directory does not exist: " + dir);
		}
	}

	if (validDirs.empty()) {
		return false;
	}

	m_directories = std::move(validDirs);
	m_callback = std::move(callback);
	m_pendingEvents.clear();

#if defined(__linux__)
	if (pipe2(m_wakeupPipe, O_NONBLOCK | O_CLOEXEC) != 0) {
		m_wakeupPipe[0] = -1;
		m_wakeupPipe[1] = -1;
	}
#elif defined(_WIN32)
	m_stopEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
#endif

	m_running.store(true, std::memory_order_release);
	m_thread = std::thread(&FileWatcher::runLoop, this);
	return true;
}

void FileWatcher::stop() {
	const bool wasRunning = m_running.exchange(false, std::memory_order_acq_rel);
	if (!wasRunning && !m_thread.joinable()) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(m_stopMutex);
		m_stopCv.notify_all();
	}

#if defined(__linux__)
	if (m_wakeupPipe[1] >= 0) {
		const char byte = 1;
		const ssize_t ignored = ::write(m_wakeupPipe[1], &byte, 1);
		(void)ignored;
	}
#elif defined(_WIN32)
	if (m_stopEvent != nullptr) {
		SetEvent(static_cast<HANDLE>(m_stopEvent));
	}
#endif

	if (m_thread.joinable()) {
		m_thread.join();
	}

#if defined(__linux__)
	if (m_wakeupPipe[0] >= 0) {
		::close(m_wakeupPipe[0]);
		m_wakeupPipe[0] = -1;
	}
	if (m_wakeupPipe[1] >= 0) {
		::close(m_wakeupPipe[1]);
		m_wakeupPipe[1] = -1;
	}
#elif defined(_WIN32)
	if (m_stopEvent != nullptr) {
		CloseHandle(static_cast<HANDLE>(m_stopEvent));
		m_stopEvent = nullptr;
	}
#endif

	m_pendingEvents.clear();
}

void FileWatcher::setDebounceInterval(std::chrono::milliseconds interval) {
	std::lock_guard<std::mutex> lock(m_configMutex);
	m_debounceInterval = interval;
}

std::chrono::milliseconds FileWatcher::getDebounceInterval() const {
	std::lock_guard<std::mutex> lock(m_configMutex);
	return m_debounceInterval;
}

void FileWatcher::recordRawChange(const std::string& rawPath) {
	if (isIgnoredHotReloadFile(rawPath)) {
		return;
	}
	const std::string normalized = normalizeAssetPath(rawPath);
	if (normalized.empty() || isIgnoredHotReloadFile(normalized)) {
		return;
	}
	m_pendingEvents[normalized] = Clock::now();
}

void FileWatcher::flushDebouncedEvents() {
	if (m_pendingEvents.empty()) {
		return;
	}

	const auto debounce = getDebounceInterval();
	const auto now = Clock::now();

	std::vector<std::string> readyPaths;
	for (auto it = m_pendingEvents.begin(); it != m_pendingEvents.end();) {
		if (now - it->second >= debounce) {
			readyPaths.push_back(it->first);
			it = m_pendingEvents.erase(it);
		} else {
			++it;
		}
	}

	for (const std::string& path : readyPaths) {
		std::error_code ec;
		if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) {
			continue;
		}
		if (m_callback) {
			m_callback(path);
		}
	}
}

void FileWatcher::runLoop() {
#if defined(__linux__)
	runInotifyLoop();
#elif defined(_WIN32)
	runWindowsLoop();
#else
	runPollingLoop();
#endif
}

#if defined(__linux__)
void FileWatcher::runInotifyLoop() {
	const int inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
	if (inotifyFd < 0) {
		platform::log(
			platform::LogLevel::Warning,
			"[HotReload] inotify_init1 failed; falling back to polling watcher");
		runPollingLoop();
		return;
	}

	constexpr uint32_t kWatchMask =
		IN_CLOSE_WRITE | IN_MODIFY | IN_MOVED_TO | IN_CREATE;

	std::unordered_map<int, fs::path> watchDescriptors;
	auto addWatch = [&](const fs::path& dirPath) {
		const int wd =
			inotify_add_watch(inotifyFd, dirPath.c_str(), kWatchMask);
		if (wd >= 0) {
			watchDescriptors[wd] = dirPath;
		}
	};

	auto addWatchRecursive = [&](const fs::path& rootDir) {
		addWatch(rootDir);
		std::error_code ec;
		for (auto it = fs::recursive_directory_iterator(
				 rootDir, fs::directory_options::skip_permission_denied, ec);
			 !ec && it != fs::recursive_directory_iterator();
			 it.increment(ec)) {
			if (it->is_directory(ec)) {
				addWatch(it->path());
			}
		}
	};

	for (const std::string& dir : m_directories) {
		addWatchRecursive(fs::path(dir));
	}

	alignas(struct inotify_event) char buffer[8192];

	while (m_running.load(std::memory_order_acquire)) {
		struct pollfd fds[2];
		nfds_t nfds = 1;
		fds[0].fd = inotifyFd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

		if (m_wakeupPipe[0] >= 0) {
			fds[1].fd = m_wakeupPipe[0];
			fds[1].events = POLLIN;
			fds[1].revents = 0;
			nfds = 2;
		}

		const int timeoutMs = static_cast<int>(m_pollInterval.count());
		const int ready = ::poll(fds, nfds, timeoutMs);

		if (ready > 0) {
			if (nfds == 2 && (fds[1].revents & POLLIN)) {
				break;	// Wakeup requested by stop()
			}
			if (fds[0].revents & POLLIN) {
				while (true) {
					const ssize_t length =
						::read(inotifyFd, buffer, sizeof(buffer));
					if (length <= 0) {
						if (length < 0 &&
							(errno == EAGAIN || errno == EWOULDBLOCK)) {
							break;
						}
						break;
					}

					ssize_t offset = 0;
					while (offset < length) {
						const auto* event =
							reinterpret_cast<const struct inotify_event*>(
								buffer + offset);
						offset += static_cast<ssize_t>(
							sizeof(struct inotify_event) + event->len);

						if (event->len == 0) continue;
						const auto dirIt = watchDescriptors.find(event->wd);
						if (dirIt == watchDescriptors.end()) continue;

						const fs::path fullPath =
							dirIt->second / std::string(event->name);
						if (event->mask & IN_ISDIR) {
							if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
								addWatchRecursive(fullPath);
							}
						} else {
							recordRawChange(fullPath.string());
						}
					}
				}
			}
		}

		flushDebouncedEvents();
	}

	for (const auto& [wd, _] : watchDescriptors) {
		inotify_rm_watch(inotifyFd, wd);
	}
	::close(inotifyFd);
}
#endif

#if defined(_WIN32)
void FileWatcher::runWindowsLoop() {
	struct DirWatch {
		fs::path root;
		HANDLE dirHandle = INVALID_HANDLE_VALUE;
		OVERLAPPED overlapped{};
		std::vector<char> buffer;
		bool pending = false;
	};

	std::vector<DirWatch> watches;
	watches.reserve(m_directories.size());

	for (const std::string& dir : m_directories) {
		HANDLE handle = CreateFileA(
			dir.c_str(), FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			nullptr);
		if (handle == INVALID_HANDLE_VALUE) continue;

		DirWatch w;
		w.root = fs::path(dir);
		w.dirHandle = handle;
		w.overlapped.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
		w.buffer.resize(16384);
		watches.push_back(std::move(w));
	}

	if (watches.empty()) {
		runPollingLoop();
		return;
	}

	auto armWatch = [](DirWatch& w) {
		ResetEvent(w.overlapped.hEvent);
		DWORD bytesReturned = 0;
		const BOOL ok = ReadDirectoryChangesW(
			w.dirHandle, w.buffer.data(), static_cast<DWORD>(w.buffer.size()),
			TRUE,
			FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME |
				FILE_NOTIFY_CHANGE_SIZE,
			&bytesReturned, &w.overlapped, nullptr);
		w.pending = (ok != 0);
	};

	for (auto& w : watches) {
		armWatch(w);
	}

	std::vector<HANDLE> waitHandles;
	waitHandles.reserve(watches.size() + 1);
	waitHandles.push_back(static_cast<HANDLE>(m_stopEvent));
	for (const auto& w : watches) {
		waitHandles.push_back(w.overlapped.hEvent);
	}

	while (m_running.load(std::memory_order_acquire)) {
		const DWORD waitMs = static_cast<DWORD>(m_pollInterval.count());
		const DWORD res = WaitForMultipleObjects(
			static_cast<DWORD>(waitHandles.size()), waitHandles.data(), FALSE,
			waitMs);

		if (res == WAIT_OBJECT_0) {
			break;	// Stop signaled
		}

		if (res > WAIT_OBJECT_0 &&
			res < WAIT_OBJECT_0 + static_cast<DWORD>(waitHandles.size())) {
			const size_t idx = static_cast<size_t>(res - WAIT_OBJECT_0 - 1);
			DirWatch& w = watches[idx];
			DWORD bytesTransferred = 0;
			if (GetOverlappedResult(w.dirHandle, &w.overlapped,
									&bytesTransferred, FALSE) &&
				bytesTransferred > 0) {
				const char* ptr = w.buffer.data();
				while (true) {
					const auto* info =
						reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(ptr);
					std::wstring wideName(info->FileName,
										  info->FileNameLength / sizeof(WCHAR));
					const fs::path fullPath = w.root / fs::path(wideName);
					recordRawChange(fullPath.string());
					if (info->NextEntryOffset == 0) break;
					ptr += info->NextEntryOffset;
				}
			}
			armWatch(w);
		}

		flushDebouncedEvents();
	}

	for (auto& w : watches) {
		CancelIo(w.dirHandle);
		CloseHandle(w.overlapped.hEvent);
		CloseHandle(w.dirHandle);
	}
}
#endif

void FileWatcher::runPollingLoop() {
	std::unordered_map<std::string, fs::file_time_type> knownTimes;

	auto scanDirectories = [&](bool recordChanges) {
		for (const std::string& dir : m_directories) {
			std::error_code ec;
			for (auto it = fs::recursive_directory_iterator(
					 dir, fs::directory_options::skip_permission_denied, ec);
				 !ec && it != fs::recursive_directory_iterator();
				 it.increment(ec)) {
				if (!it->is_regular_file(ec)) continue;
				const std::string pathStr = it->path().string();
				if (isIgnoredHotReloadFile(pathStr)) continue;

				const auto writeTime = it->last_write_time(ec);
				if (ec) continue;

				auto found = knownTimes.find(pathStr);
				if (found == knownTimes.end()) {
					knownTimes.emplace(pathStr, writeTime);
					if (recordChanges) {
						recordRawChange(pathStr);
					}
				} else if (found->second != writeTime) {
					found->second = writeTime;
					if (recordChanges) {
						recordRawChange(pathStr);
					}
				}
			}
		}
	};

	// Seed baseline timestamps without firing events for pre-existing files.
	scanDirectories(false);

	while (m_running.load(std::memory_order_acquire)) {
		{
			std::unique_lock<std::mutex> lock(m_stopMutex);
			m_stopCv.wait_for(lock, m_pollInterval, [this]() {
				return !m_running.load(std::memory_order_acquire);
			});
		}
		if (!m_running.load(std::memory_order_acquire)) break;

		scanDirectories(true);
		flushDebouncedEvents();
	}
}

}  // namespace bird::hotreload
