#include "BuildToolsRunner.h"

#include <cstring>
#include <filesystem>
#include <sstream>

#include "../platform/Log.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace bird::hotreload {

BuildToolsRunner::BuildToolsRunner(std::string buildToolsPath,
								   ProcessExecutor executor)
	: m_buildToolsPath(buildToolsPath.empty() ? discoverBuildToolsPath()
											  : std::move(buildToolsPath)),
	  m_executor(executor ? std::move(executor) : &BuildToolsRunner::runProcess) {}

BuildToolsRunner::~BuildToolsRunner() {
	stop();
}

void BuildToolsRunner::start() {
	bool expected = false;
	if (!m_running.compare_exchange_strong(expected, true,
										   std::memory_order_acq_rel)) {
		return;
	}
	m_worker = std::thread(&BuildToolsRunner::workerLoop, this);
}

void BuildToolsRunner::stop() {
	const bool wasRunning = m_running.exchange(false, std::memory_order_acq_rel);
	if (!wasRunning && !m_worker.joinable()) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_jobs.clear();
	}
	m_cv.notify_all();
	if (m_worker.joinable()) {
		m_worker.join();
	}
}

void BuildToolsRunner::setBuildToolsPath(std::string path) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_buildToolsPath =
		path.empty() ? discoverBuildToolsPath() : std::move(path);
}

void BuildToolsRunner::enqueueFileBuild(const std::string& sourcePath,
										BuildCompleteCallback onComplete) {
	BuildJob job;
	job.targetPath = sourcePath;
	job.args = {"--file", sourcePath, "--force"};
	job.onComplete = std::move(onComplete);
	enqueueJob(std::move(job));
}

void BuildToolsRunner::enqueueDirectoryBuild(const std::string& directoryPath,
											 BuildCompleteCallback onComplete) {
	BuildJob job;
	job.targetPath = directoryPath;
	job.args = {"--dir", directoryPath};
	job.onComplete = std::move(onComplete);
	enqueueJob(std::move(job));
}

void BuildToolsRunner::enqueueJob(BuildJob job) {
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (BuildJob& existing : m_jobs) {
			if (existing.targetPath == job.targetPath &&
				existing.args == job.args) {
				existing.onComplete = std::move(job.onComplete);
				return;
			}
		}
		m_jobs.push_back(std::move(job));
	}
	m_cv.notify_one();
}

size_t BuildToolsRunner::pendingJobCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_jobs.size();
}

void BuildToolsRunner::workerLoop() {
	while (true) {
		BuildJob job;
		std::string executable;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this]() {
				return !m_running.load(std::memory_order_acquire) ||
					   !m_jobs.empty();
			});
			if (!m_running.load(std::memory_order_acquire)) {
				break;
			}
			job = std::move(m_jobs.front());
			m_jobs.pop_front();
			executable = m_buildToolsPath;
		}

		BuildResult result = m_executor(executable, job.args);
		result.assetPath = job.targetPath;

		if (!result.success) {
			std::ostringstream msg;
			msg << "[HotReload] BuildTools failed for '" << job.targetPath
				<< "' (exit code " << result.exitCode << "). "
				<< "Keeping previous live asset active.";
			if (!result.stderrOutput.empty()) {
				msg << "\n  stderr: " << result.stderrOutput;
			}
			if (!result.stdoutOutput.empty()) {
				msg << "\n  stdout: " << result.stdoutOutput;
			}
			platform::log(platform::LogLevel::Error, msg.str());
		} else {
			platform::log(platform::LogLevel::Info,
						  "[HotReload] Recompiled asset: " + job.targetPath);
		}

		if (job.onComplete) {
			job.onComplete(result);
		}
	}
}

std::string BuildToolsRunner::discoverBuildToolsPath() {
#if defined(_WIN32)
	const std::vector<std::string> candidates = {
		"./BuildTools.exe", "./build/BuildTools.exe",
		"./build/Debug/BuildTools.exe", "./build/Release/BuildTools.exe",
		"./bin/BuildTools.exe"};
#else
	const std::vector<std::string> candidates = {
		"./BuildTools", "./build/BuildTools", "./bin/BuildTools"};
#endif
	std::error_code ec;
	for (const std::string& candidate : candidates) {
		if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec)) {
			return candidate;
		}
	}
	return candidates.front();
}

#if defined(_WIN32)
namespace {

std::string quoteWindowsArg(const std::string& arg) {
	if (arg.find_first_of(" \t\"") == std::string::npos) {
		return arg;
	}
	std::string out = "\"";
	for (char c : arg) {
		if (c == '"') out += "\\\"";
		else out += c;
	}
	out += "\"";
	return out;
}

void drainHandle(HANDLE handle, std::string& output) {
	char buf[4096];
	DWORD bytesRead = 0;
	while (ReadFile(handle, buf, sizeof(buf), &bytesRead, nullptr) &&
		   bytesRead > 0) {
		output.append(buf, bytesRead);
	}
}

}  // namespace

BuildResult BuildToolsRunner::runProcess(const std::string& executable,
										 const std::vector<std::string>& args) {
	BuildResult result;
	SECURITY_ATTRIBUTES sa{};
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	HANDLE stdoutRead = nullptr, stdoutWrite = nullptr;
	HANDLE stderrRead = nullptr, stderrWrite = nullptr;
	if (!CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
		!CreatePipe(&stderrRead, &stderrWrite, &sa, 0)) {
		result.stderrOutput = "Failed to create stdout/stderr pipes";
		return result;
	}
	SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

	std::string cmdLine = quoteWindowsArg(executable);
	for (const std::string& arg : args) {
		cmdLine += " " + quoteWindowsArg(arg);
	}

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdOutput = stdoutWrite;
	si.hStdError = stderrWrite;
	si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

	PROCESS_INFORMATION pi{};
	std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
	mutableCmd.push_back('\0');

	const BOOL created = CreateProcessA(nullptr, mutableCmd.data(), nullptr,
										nullptr, TRUE, CREATE_NO_WINDOW,
										nullptr, nullptr, &si, &pi);
	CloseHandle(stdoutWrite);
	CloseHandle(stderrWrite);

	if (!created) {
		CloseHandle(stdoutRead);
		CloseHandle(stderrRead);
		result.stderrOutput =
			"Failed to launch BuildTools executable: " + executable;
		return result;
	}

	std::thread stderrThread(drainHandle, stderrRead,
							 std::ref(result.stderrOutput));
	drainHandle(stdoutRead, result.stdoutOutput);
	stderrThread.join();

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 1;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	CloseHandle(stdoutRead);
	CloseHandle(stderrRead);

	result.exitCode = static_cast<int>(exitCode);
	result.success = (result.exitCode == 0);
	return result;
}
#else
BuildResult BuildToolsRunner::runProcess(const std::string& executable,
										 const std::vector<std::string>& args) {
	BuildResult result;

	int stdoutPipe[2]{-1, -1};
	int stderrPipe[2]{-1, -1};
	if (::pipe(stdoutPipe) != 0) {
		result.stderrOutput =
			std::string("pipe(stdout) failed: ") + std::strerror(errno);
		return result;
	}
	if (::pipe(stderrPipe) != 0) {
		::close(stdoutPipe[0]);
		::close(stdoutPipe[1]);
		result.stderrOutput =
			std::string("pipe(stderr) failed: ") + std::strerror(errno);
		return result;
	}

	const pid_t pid = ::fork();
	if (pid < 0) {
		::close(stdoutPipe[0]);
		::close(stdoutPipe[1]);
		::close(stderrPipe[0]);
		::close(stderrPipe[1]);
		result.stderrOutput =
			std::string("fork() failed: ") + std::strerror(errno);
		return result;
	}

	if (pid == 0) {
		// Child process: wire stdout/stderr pipes and exec directly (no shell).
		::dup2(stdoutPipe[1], STDOUT_FILENO);
		::dup2(stderrPipe[1], STDERR_FILENO);
		::close(stdoutPipe[0]);
		::close(stdoutPipe[1]);
		::close(stderrPipe[0]);
		::close(stderrPipe[1]);

		std::vector<char*> argv;
		argv.reserve(args.size() + 2);
		argv.push_back(const_cast<char*>(executable.c_str()));
		for (const std::string& arg : args) {
			argv.push_back(const_cast<char*>(arg.c_str()));
		}
		argv.push_back(nullptr);

		::execvp(executable.c_str(), argv.data());
		const std::string errMsg = "Failed to execute '" + executable +
								   "': " + std::strerror(errno) + "\n";
		const ssize_t ignored =
			::write(STDERR_FILENO, errMsg.data(), errMsg.size());
		(void)ignored;
		::_exit(127);
	}

	// Parent process: close write ends and drain both pipes concurrently.
	::close(stdoutPipe[1]);
	::close(stderrPipe[1]);

	struct pollfd fds[2];
	fds[0].fd = stdoutPipe[0];
	fds[0].events = POLLIN;
	fds[1].fd = stderrPipe[0];
	fds[1].events = POLLIN;

	int openPipes = 2;
	char buf[4096];
	while (openPipes > 0) {
		const int ready = ::poll(fds, 2, -1);
		if (ready < 0) {
			if (errno == EINTR) continue;
			break;
		}
		for (int i = 0; i < 2; ++i) {
			if (fds[i].fd < 0) continue;
			if (fds[i].revents & (POLLIN | POLLHUP | POLLERR)) {
				const ssize_t n = ::read(fds[i].fd, buf, sizeof(buf));
				if (n > 0) {
					if (i == 0) {
						result.stdoutOutput.append(buf,
												   static_cast<size_t>(n));
					} else {
						result.stderrOutput.append(buf,
												   static_cast<size_t>(n));
					}
				} else if (n == 0 || (n < 0 && errno != EINTR)) {
					::close(fds[i].fd);
					fds[i].fd = -1;
					--openPipes;
				}
			}
		}
	}

	if (fds[0].fd >= 0) ::close(fds[0].fd);
	if (fds[1].fd >= 0) ::close(fds[1].fd);

	int status = 0;
	while (::waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR) break;
	}

	if (WIFEXITED(status)) {
		result.exitCode = WEXITSTATUS(status);
		result.success = (result.exitCode == 0);
	} else if (WIFSIGNALED(status)) {
		result.exitCode = 128 + WTERMSIG(status);
		result.success = false;
	} else {
		result.exitCode = -1;
		result.success = false;
	}

	return result;
}
#endif

}  // namespace bird::hotreload
