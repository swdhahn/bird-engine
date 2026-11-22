#ifndef BIRD_HOT_RELOAD_TYPES_H
#define BIRD_HOT_RELOAD_TYPES_H

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace bird::hotreload {

/**
 * Classification of a watched source asset for hot-reload routing.
 *
 * Determines whether a modified file must be compiled by BuildTools before
 * reloading, whether it triggers a multi-shader rebuild (GLSL include), or
 * whether it notifies the Dart scripting runtime directly.
 */
enum class HotReloadAssetKind : uint8_t {
	/** Unrecognized or ignored file type (e.g. .bird, .tmp, editor swap files). */
	Unknown = 0,
	/** Individual GLSL shader stage (.vert, .frag, .geom, .comp, .tesc, .tese). */
	Shader,
	/** Shared GLSL header (.glsl) that may be #included by multiple shaders. */
	ShaderInclude,
	/** 2D image texture (.png, .jpg, .jpeg, .tga, .bmp, .psd, .gif, .hdr, .pic, .pnm). */
	Texture,
	/** Material definition (.mtl, .pbr, .mat). */
	Material,
	/** TrueType / OpenType font (.ttf, .otf). */
	Font,
	/** 3D model (.obj, .fbx, .gltf, .glb). */
	Model,
	/** Dart gameplay script (.dart). */
	DartScript
};

/**
 * Logical identifier for an asset tracked by the hot-reload system.
 *
 * Uses a normalized forward-slash relative path (e.g. "assets/shaders/mesh.vert")
 * that matches the cache keys stored inside bird::ResourceManager.
 */
struct AssetID {
	/** Normalized relative path to the raw source asset (e.g. "assets/textures/wood.png"). */
	std::string sourcePath;

	/** Classified asset category used to route reload handling. */
	HotReloadAssetKind kind = HotReloadAssetKind::Unknown;

	bool operator==(const AssetID& other) const {
		return sourcePath == other.sourcePath && kind == other.kind;
	}

	bool operator!=(const AssetID& other) const { return !(*this == other); }

	/** @return True when this AssetID refers to a valid, classified asset. */
	bool isValid() const {
		return !sourcePath.empty() && kind != HotReloadAssetKind::Unknown;
	}
};

/**
 * Message pushed from the background build pipeline to the main thread
 * indicating that a compiled asset is ready to be swapped in place.
 */
struct AssetReloadRequest {
	/** Logical identifier of the asset to reload. */
	AssetID assetId;

	/** Path to the freshly baked binary asset (e.g. "<sourcePath>.bird"), if applicable. */
	std::string compiledPath;

	/** Monotonic sequence counter incremented on every enqueued reload. */
	uint64_t generation = 0;
};

/**
 * Result of invoking the external BuildTools compiler on a modified asset.
 */
struct BuildResult {
	/** True when BuildTools exited with status 0. */
	bool success = false;

	/** Process exit code (-1 if the process failed to launch). */
	int exitCode = -1;

	/** Captured standard output from BuildTools. */
	std::string stdoutOutput;

	/** Captured standard error from BuildTools. */
	std::string stderrOutput;

	/** Source asset path that was compiled. */
	std::string assetPath;
};

/**
 * Runtime configuration for the hot-reload subsystem.
 */
struct HotReloadConfig {
	/** Root directory containing raw source assets to monitor recursively. */
	std::string watchDirectory = "assets";

	/**
	 * Path to the BuildTools executable.
	 * If empty, BuildToolsRunner searches common locations ("./BuildTools",
	 * "./build/BuildTools", and the current executable's directory).
	 */
	std::string buildToolsPath;

	/**
	 * Optional directory containing Dart scripts to monitor for hot-reload hooks.
	 * Leave empty to watch only watchDirectory.
	 */
	std::string dartWatchDirectory;

	/**
	 * Minimum quiet period after the last filesystem event for a given path
	 * before triggering recompilation. Coalesces rapid editor saves and partial
	 * writes. Defaults to 100 ms.
	 */
	std::chrono::milliseconds debounceInterval{100};

	/**
	 * Polling interval used by the portable fallback watcher backend (and as
	 * the maximum wait slice for event-driven backends to flush debounced items).
	 */
	std::chrono::milliseconds pollInterval{25};
};

/**
 * Callback invoked on the main thread when a watched .dart script is modified.
 * @param scriptPath Normalized path to the modified Dart file.
 */
using DartReloadCallback = std::function<void(const std::string& scriptPath)>;

/**
 * Returns true if the filename looks like a baked output, temporary file, or
 * editor swap/backup artifact that should never trigger hot-reload.
 *
 * @param path File path to inspect.
 * @return True when the file should be ignored by the watcher.
 */
inline bool isIgnoredHotReloadFile(const std::string& path) {
	if (path.empty()) return true;
	const std::filesystem::path fsPath(path);
	const std::string filename = fsPath.filename().string();
	if (filename.empty()) return true;

	// Hidden files, Vim swap files, Emacs lock/autosave files, backup files.
	if (filename[0] == '.' || filename[0] == '#' || filename.back() == '~') {
		return true;
	}

	std::string ext = fsPath.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (ext == ".bird" || ext == ".tmp" || ext == ".swp" || ext == ".swo" ||
		ext == ".bak" || ext == ".orig" || ext == ".part") {
		return true;
	}
	return false;
}

/**
 * Classifies a source file path into its HotReloadAssetKind by extension.
 *
 * Returns HotReloadAssetKind::Unknown for baked `.bird` outputs, temporary
 * files, and unrecognized extensions.
 *
 * @param path File path to classify.
 * @return Classified asset kind.
 */
inline HotReloadAssetKind classifyAssetPath(const std::string& path) {
	if (isIgnoredHotReloadFile(path)) return HotReloadAssetKind::Unknown;

	std::string ext = std::filesystem::path(path).extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(),
				   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (ext == ".vert" || ext == ".frag" || ext == ".geom" || ext == ".comp" ||
		ext == ".tesc" || ext == ".tese") {
		return HotReloadAssetKind::Shader;
	}
	if (ext == ".glsl") {
		return HotReloadAssetKind::ShaderInclude;
	}
	if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
		ext == ".bmp" || ext == ".psd" || ext == ".gif" || ext == ".hdr" ||
		ext == ".pic" || ext == ".pnm") {
		return HotReloadAssetKind::Texture;
	}
	if (ext == ".mtl" || ext == ".pbr" || ext == ".mat") {
		return HotReloadAssetKind::Material;
	}
	if (ext == ".ttf" || ext == ".otf") {
		return HotReloadAssetKind::Font;
	}
	if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
		return HotReloadAssetKind::Model;
	}
	if (ext == ".dart") {
		return HotReloadAssetKind::DartScript;
	}
	return HotReloadAssetKind::Unknown;
}

/**
 * Normalizes a filesystem path to use forward slashes and strips leading "./".
 *
 * When a non-empty baseDirectory is provided and the path is absolute, attempts
 * to express the path relative to the current working directory so it matches
 * ResourceManager cache keys (e.g. "assets/shaders/mesh.vert").
 *
 * @param rawPath Path reported by the OS file watcher.
 * @return Normalized forward-slash path string.
 */
inline std::string normalizeAssetPath(const std::string& rawPath) {
	std::error_code ec;
	std::filesystem::path p(rawPath);
	if (p.is_absolute()) {
		const auto cwd = std::filesystem::current_path(ec);
		if (!ec) {
			const auto rel = std::filesystem::relative(p, cwd, ec);
			if (!ec && !rel.empty() && rel.native().rfind("..", 0) != 0) {
				p = rel;
			}
		}
	}
	std::string normalized = p.lexically_normal().generic_string();
	while (normalized.rfind("./", 0) == 0) {
		normalized.erase(0, 2);
	}
	return normalized;
}

}  // namespace bird::hotreload

#endif	// BIRD_HOT_RELOAD_TYPES_H
