#ifndef BIRD_HOT_RELOAD_BUILD_TOOLS_RUNNER_H
#define BIRD_HOT_RELOAD_BUILD_TOOLS_RUNNER_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "HotReloadTypes.h"

namespace bird::hotreload {

/**
 * Callback invoked on the BuildToolsRunner worker thread when an asynchronous
 * compilation job finishes.
 *
 * @param result Captured exit code, stdout, stderr, and target asset path.
 */
using BuildCompleteCallback = std::function<void(const BuildResult& result)>;

/**
 * Injectable process execution strategy used by BuildToolsRunner.
 *
 * Defaults to BuildToolsRunner::runProcess(). Can be overridden in unit tests
 * to simulate BuildTools success, compilation errors, or slow builds without
 * spawning external binaries.
 *
 * @param executable Path to the executable to run.
 * @param args Argument list (excluding argv[0]).
 * @return Captured BuildResult.
 */
using ProcessExecutor = std::function<BuildResult(
	const std::string& executable, const std::vector<std::string>& args)>;

/**
 * Asynchronous external `BuildTools` process runner.
 *
 * Executes the `BuildTools` executable on a dedicated background worker thread
 * to recompile only the modified source asset (or a shader directory when a
 * shared `.glsl` header changes). Captures both stdout and stderr without
 * invoking a shell.
 *
 * If compilation fails (non-zero exit code or launch error), logs the captured
 * diagnostics clearly and reports failure via BuildResult without crashing or
 * touching live engine assets.
 *
 * Pending jobs targeting the same path are deduplicated while queued so rapid
 * edits never race to write the same `.bird` file.
 */
class BuildToolsRunner {
   public:
	/**
	 * Constructs a BuildToolsRunner.
	 *
	 * @param buildToolsPath Path to the BuildTools executable. When empty,
	 *        discoverBuildToolsPath() is used to locate it automatically.
	 * @param executor Optional custom process executor (defaults to runProcess).
	 */
	explicit BuildToolsRunner(std::string buildToolsPath = "",
							  ProcessExecutor executor = nullptr);

	/** Stops the worker thread and drains any remaining unstarted jobs. */
	~BuildToolsRunner();

	BuildToolsRunner(const BuildToolsRunner&) = delete;
	BuildToolsRunner& operator=(const BuildToolsRunner&) = delete;

	/**
	 * Starts the background compilation worker thread.
	 * Safe to call if already running (no-op).
	 */
	void start();

	/**
	 * Signals the worker thread to exit after finishing the current job,
	 * clears queued unstarted jobs, and joins the thread.
	 */
	void stop();

	/** @return True while the worker thread is active. */
	bool isRunning() const { return m_running.load(std::memory_order_acquire); }

	/**
	 * Enqueues an asynchronous single-file recompile:
	 *   `BuildTools --file <sourcePath> --force`
	 *
	 * @param sourcePath Normalized path to the modified source asset.
	 * @param onComplete Callback invoked when the build finishes.
	 */
	void enqueueFileBuild(const std::string& sourcePath,
						  BuildCompleteCallback onComplete);

	/**
	 * Enqueues an asynchronous incremental directory recompile:
	 *   `BuildTools --dir <directoryPath>`
	 *
	 * Used when a shared `.glsl` include file changes so BuildTools rebakes
	 * only shaders whose transitive include hash is stale.
	 *
	 * @param directoryPath Path to the directory to compile incrementally.
	 * @param onComplete Callback invoked when the build finishes.
	 */
	void enqueueDirectoryBuild(const std::string& directoryPath,
							   BuildCompleteCallback onComplete);

	/** @return Number of jobs currently queued (excluding any actively running job). */
	size_t pendingJobCount() const;

	/** @return Configured or auto-discovered path to the BuildTools executable. */
	const std::string& getBuildToolsPath() const { return m_buildToolsPath; }

	/**
	 * Updates the path to the BuildTools executable.
	 *
	 * @param path New executable path.
	 */
	void setBuildToolsPath(std::string path);

	/**
	 * Searches common relative build locations for the BuildTools executable.
	 *
	 * @return Path to an existing BuildTools binary, or "./BuildTools" as fallback.
	 */
	static std::string discoverBuildToolsPath();

	/**
	 * Synchronously executes a child process without a shell, capturing both
	 * stdout and stderr concurrently to avoid pipe buffer deadlocks.
	 *
	 * @param executable Path to the executable.
	 * @param args Command-line arguments (excluding argv[0]).
	 * @return BuildResult containing success flag, exitCode, stdout, and stderr.
	 */
	static BuildResult runProcess(const std::string& executable,
								  const std::vector<std::string>& args);

   private:
	struct BuildJob {
		std::string targetPath;
		std::vector<std::string> args;
		BuildCompleteCallback onComplete;
	};

	void enqueueJob(BuildJob job);
	void workerLoop();

	std::string m_buildToolsPath;
	ProcessExecutor m_executor;

	mutable std::mutex m_mutex;
	std::condition_variable m_cv;
	std::deque<BuildJob> m_jobs;

	std::atomic<bool> m_running{false};
	std::thread m_worker;
};

}  // namespace bird::hotreload

#endif	// BIRD_HOT_RELOAD_BUILD_TOOLS_RUNNER_H
