#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "hotreload/BuildToolsRunner.h"
#include "hotreload/EntityHandleRegistry.h"
#include "hotreload/FileWatcher.h"
#include "hotreload/HotReloadManager.h"
#include "hotreload/HotReloadTypes.h"
#include "hotreload/ReloadQueue.h"

namespace fs = std::filesystem;
using namespace bird::hotreload;
using namespace std::chrono_literals;

namespace {

/** RAII temporary directory for isolated filesystem watcher tests. */
class TempDir {
   public:
	TempDir() {
		static std::atomic<uint64_t> counter{0};
		const auto id = counter.fetch_add(1, std::memory_order_relaxed);
		m_path = fs::temp_directory_path() /
				 ("bird_hotreload_test_" + std::to_string(id) + "_" +
				  std::to_string(
					  std::chrono::steady_clock::now().time_since_epoch().count()));
		fs::create_directories(m_path);
	}
	~TempDir() {
		std::error_code ec;
		fs::remove_all(m_path, ec);
	}
	const fs::path& path() const { return m_path; }

   private:
	fs::path m_path;
};

void writeTextFile(const fs::path& path, const std::string& content) {
	fs::create_directories(path.parent_path());
	std::ofstream out(path, std::ios::out | std::ios::trunc);
	out << content;
	out.flush();
}

}  // namespace

// --- 1. Classification & Normalization ---

TEST(HotReloadTypes, ClassifiesSupportedSourceExtensions) {
	EXPECT_EQ(classifyAssetPath("assets/shaders/mesh.vert"),
			  HotReloadAssetKind::Shader);
	EXPECT_EQ(classifyAssetPath("assets/shaders/gbuffer.FRAG"),
			  HotReloadAssetKind::Shader);
	EXPECT_EQ(classifyAssetPath("assets/shaders/include/pbr.glsl"),
			  HotReloadAssetKind::ShaderInclude);
	EXPECT_EQ(classifyAssetPath("assets/textures/wood.png"),
			  HotReloadAssetKind::Texture);
	EXPECT_EQ(classifyAssetPath("assets/models/hero.glb"),
			  HotReloadAssetKind::Model);
	EXPECT_EQ(classifyAssetPath("assets/fonts/DejaVuSans.ttf"),
			  HotReloadAssetKind::Font);
	EXPECT_EQ(classifyAssetPath("assets/materials/stone.pbr"),
			  HotReloadAssetKind::Material);
	EXPECT_EQ(classifyAssetPath("scripts/player.dart"),
			  HotReloadAssetKind::DartScript);
}

TEST(HotReloadTypes, IgnoresBakedOutputsAndEditorTempFiles) {
	EXPECT_EQ(classifyAssetPath("assets/shaders/mesh.vert.bird"),
			  HotReloadAssetKind::Unknown);
	EXPECT_EQ(classifyAssetPath("assets/textures/wood.png.bird"),
			  HotReloadAssetKind::Unknown);
	EXPECT_EQ(classifyAssetPath("assets/shaders/.mesh.vert.swp"),
			  HotReloadAssetKind::Unknown);
	EXPECT_EQ(classifyAssetPath("assets/shaders/mesh.vert~"),
			  HotReloadAssetKind::Unknown);
	EXPECT_EQ(classifyAssetPath("assets/shaders/#mesh.vert#"),
			  HotReloadAssetKind::Unknown);
	EXPECT_EQ(classifyAssetPath("assets/shaders/mesh.vert.tmp"),
			  HotReloadAssetKind::Unknown);
}

// --- 2. Thread-Safe ReloadQueue ---

TEST(ReloadQueue, DeduplicatesByAssetIdAndPreservesLatestGeneration) {
	ReloadQueue queue;
	EXPECT_TRUE(queue.empty());

	queue.push({{"assets/shaders/mesh.vert", HotReloadAssetKind::Shader},
				"assets/shaders/mesh.vert.bird",
				0});
	queue.push({{"assets/textures/wood.png", HotReloadAssetKind::Texture},
				"assets/textures/wood.png.bird",
				0});
	// Duplicate push for mesh.vert should coalesce and bump generation.
	queue.push({{"assets/shaders/mesh.vert", HotReloadAssetKind::Shader},
				"assets/shaders/mesh.vert.bird",
				0});

	EXPECT_EQ(queue.size(), 2u);
	const auto batch = queue.drain();
	EXPECT_TRUE(queue.empty());
	ASSERT_EQ(batch.size(), 2u);
	EXPECT_EQ(batch[0].assetId.sourcePath, "assets/shaders/mesh.vert");
	EXPECT_EQ(batch[0].generation, 3u);
	EXPECT_EQ(batch[1].assetId.sourcePath, "assets/textures/wood.png");
	EXPECT_EQ(batch[1].generation, 2u);
}

TEST(ReloadQueue, SupportsConcurrentProducers) {
	ReloadQueue queue;
	constexpr int kThreads = 4;
	constexpr int kItemsPerThread = 50;

	std::vector<std::thread> producers;
	for (int t = 0; t < kThreads; ++t) {
		producers.emplace_back([&queue, t]() {
			for (int i = 0; i < kItemsPerThread; ++i) {
				AssetReloadRequest req;
				req.assetId = {"assets/tex_" + std::to_string(t) + "_" +
								   std::to_string(i) + ".png",
							   HotReloadAssetKind::Texture};
				queue.push(std::move(req));
			}
		});
	}
	for (auto& th : producers) th.join();

	const auto drained = queue.drain();
	EXPECT_EQ(drained.size(), static_cast<size_t>(kThreads * kItemsPerThread));
}

// --- 3. Background FileWatcher & Debouncing ---

TEST(FileWatcher, DebouncesRapidSavesIntoSingleEvent) {
	TempDir dir;
	const fs::path shaderPath = dir.path() / "shaders" / "test.frag";
	writeTextFile(shaderPath, "// initial\n");

	std::mutex mtx;
	std::condition_variable cv;
	std::vector<std::string> events;

	FileWatcher watcher(100ms, 15ms);
	ASSERT_TRUE(watcher.start(dir.path().string(), [&](const std::string& p) {
		std::lock_guard<std::mutex> lock(mtx);
		events.push_back(p);
		cv.notify_all();
	}));

	// Simulate rapid editor saves within the 100ms debounce window.
	for (int i = 0; i < 5; ++i) {
		writeTextFile(shaderPath, "// edit " + std::to_string(i) + "\n");
		std::this_thread::sleep_for(15ms);
	}

	// Wait for the debounced event to arrive.
	{
		std::unique_lock<std::mutex> lock(mtx);
		const bool received =
			cv.wait_for(lock, 2s, [&]() { return !events.empty(); });
		ASSERT_TRUE(received) << "Timed out waiting for debounced file event";
	}

	// Give a small extra window to verify no duplicate events fire.
	std::this_thread::sleep_for(160ms);
	watcher.stop();

	std::lock_guard<std::mutex> lock(mtx);
	ASSERT_EQ(events.size(), 1u);
	EXPECT_NE(events[0].find("test.frag"), std::string::npos);
}

TEST(FileWatcher, IgnoresBakedBirdAndTempFiles) {
	TempDir dir;
	std::atomic<int> count{0};

	FileWatcher watcher(60ms, 15ms);
	ASSERT_TRUE(watcher.start(dir.path().string(),
							  [&](const std::string&) { count.fetch_add(1); }));

	writeTextFile(dir.path() / "mesh.vert.bird", "baked binary");
	writeTextFile(dir.path() / "mesh.vert.tmp", "temp data");
	writeTextFile(dir.path() / ".mesh.vert.swp", "swap data");

	std::this_thread::sleep_for(200ms);
	watcher.stop();

	EXPECT_EQ(count.load(), 0);
}

// --- 4. BuildToolsRunner Subprocess Capture & Failure Isolation ---

#if !defined(_WIN32)
TEST(BuildToolsRunner, CapturesStdoutAndStderrFromRealSubprocess) {
	const BuildResult ok = BuildToolsRunner::runProcess(
		"/bin/sh", {"-c", "echo 'compiled ok'; echo 'warn msg' >&2; exit 0"});
	EXPECT_TRUE(ok.success);
	EXPECT_EQ(ok.exitCode, 0);
	EXPECT_NE(ok.stdoutOutput.find("compiled ok"), std::string::npos);
	EXPECT_NE(ok.stderrOutput.find("warn msg"), std::string::npos);

	const BuildResult fail = BuildToolsRunner::runProcess(
		"/bin/sh", {"-c", "echo 'syntax error line 42' >&2; exit 1"});
	EXPECT_FALSE(fail.success);
	EXPECT_EQ(fail.exitCode, 1);
	EXPECT_NE(fail.stderrOutput.find("syntax error line 42"), std::string::npos);
}
#endif

// --- 5. HotReloadManager End-to-End Flow ---

TEST(HotReloadManager, EnqueuesReloadOnlyWhenBuildSucceeds) {
	TempDir dir;
	std::atomic<bool> shouldSucceed{false};
	std::atomic<int> buildCalls{0};

	ProcessExecutor mockExecutor =
		[&](const std::string&,
			const std::vector<std::string>& args) -> BuildResult {
		BuildResult r;
		if (shouldSucceed.load()) {
			r.success = true;
			r.exitCode = 0;
			r.stdoutOutput = "Compiling Shader: ok\n";
		} else {
			r.success = false;
			r.exitCode = 1;
			r.stderrOutput = "FAILED: GLSL syntax error\n";
		}
		if (args.size() >= 2) r.assetPath = args[1];
		buildCalls.fetch_add(1);
		return r;
	};

	HotReloadConfig cfg;
	cfg.watchDirectory = dir.path().string();
	cfg.buildToolsPath = "mock-BuildTools";
	cfg.debounceInterval = 50ms;
	cfg.pollInterval = 10ms;

	HotReloadManager manager(cfg, mockExecutor);
	ASSERT_TRUE(manager.start());

	// 1) Failing build must NOT enqueue an AssetReloadRequest.
	manager.notifyFileChanged("assets/shaders/broken.frag");
	{
		const auto deadline = std::chrono::steady_clock::now() + 2s;
		while (manager.getFailedBuildCount() < 1 &&
			   std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(5ms);
		}
		ASSERT_EQ(manager.getFailedBuildCount(), 1u);
	}
	EXPECT_EQ(manager.getSuccessfulBuildCount(), 0u);
	EXPECT_NE(manager.getLastBuildError().find("GLSL syntax error"),
			  std::string::npos);
	EXPECT_TRUE(manager.drainRequests().empty());

	// 2) Succeeding build MUST enqueue an AssetReloadRequest.
	shouldSucceed.store(true);
	manager.notifyFileChanged("assets/shaders/broken.frag");
	{
		const auto deadline = std::chrono::steady_clock::now() + 2s;
		while (manager.getSuccessfulBuildCount() < 1 &&
			   std::chrono::steady_clock::now() < deadline) {
			std::this_thread::sleep_for(5ms);
		}
		ASSERT_EQ(manager.getSuccessfulBuildCount(), 1u);
	}
	const auto requests = manager.drainRequests();
	ASSERT_EQ(requests.size(), 1u);
	EXPECT_EQ(requests[0].assetId.sourcePath, "assets/shaders/broken.frag");
	EXPECT_EQ(requests[0].assetId.kind, HotReloadAssetKind::Shader);
	EXPECT_EQ(requests[0].compiledPath, "assets/shaders/broken.frag.bird");

	// 3) Dart script change bypasses BuildTools and enqueues directly.
	manager.notifyFileChanged("scripts/game.dart");
	const auto dartReqs = manager.drainRequests();
	ASSERT_EQ(dartReqs.size(), 1u);
	EXPECT_EQ(dartReqs[0].assetId.kind, HotReloadAssetKind::DartScript);

	manager.stop();
}

// --- 6. EntityHandleRegistry (Dart Hot-Reload Safety) ---

TEST(EntityHandleRegistry, ResolvesLiveEntitiesAndReturnsNullAfterDestroy) {
	auto& registry = EntityHandleRegistry::get();
	registry.clear();

	int dummyStorage = 42;
	auto* fakeEntity = reinterpret_cast<bird::WorldObject*>(&dummyStorage);
	constexpr EntityID kTestId = 1001;

	registry.registerEntity(kTestId, fakeEntity);
	EXPECT_TRUE(registry.isValid(kTestId));
	EXPECT_EQ(registry.resolve(kTestId), fakeEntity);
	EXPECT_EQ(registry.getID(fakeEntity), kTestId);

	// Verify C-ABI FFI hooks match.
	EXPECT_EQ(bird_EntityRegistry_getID(static_cast<void*>(fakeEntity)),
			  kTestId);
	EXPECT_EQ(bird_EntityRegistry_resolve(kTestId),
			  static_cast<void*>(fakeEntity));
	EXPECT_TRUE(bird_EntityRegistry_isValid(kTestId));

	// Simulate native entity destruction: handle must safely resolve to nullptr.
	registry.unregisterEntity(kTestId);
	EXPECT_FALSE(registry.isValid(kTestId));
	EXPECT_EQ(registry.resolve(kTestId), nullptr);
	EXPECT_EQ(bird_EntityRegistry_resolve(kTestId), nullptr);
	EXPECT_FALSE(bird_EntityRegistry_isValid(kTestId));
	EXPECT_EQ(bird_EntityRegistry_getID(static_cast<void*>(fakeEntity)),
			  kInvalidEntityID);
}
