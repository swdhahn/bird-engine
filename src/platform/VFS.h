#ifndef BIRD_VFS_H
#define BIRD_VFS_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace bird::platform {

/**
 * Read-only access to packaged assets. Paths are relative and use '/'.
 * Desktop and iOS read the file system; Android reads from inside the APK;
 * the Web reads Emscripten's preloaded file system.
 */
class VFS {
   public:
	virtual ~VFS() = default;

	/**
	 * Reads a whole file. O(file size).
	 * @return the bytes, or std::nullopt when the file is missing or
	 *         unreadable.
	 */
	virtual std::optional<std::vector<uint8_t>> readFile(
		const std::string& path) const = 0;

	/** @return true when readFile(path) would succeed. */
	virtual bool exists(const std::string& path) const = 0;

	/** @return the process-wide VFS (a NativeVFS until replaced). */
	static const VFS& get();

	/**
	 * Installs the process-wide VFS; call before loading any asset.
	 * @throws std::invalid_argument for nullptr.
	 */
	static void set(std::unique_ptr<VFS> vfs);
};

/** Reads through the C++ file system, relative to the working directory. */
class NativeVFS final : public VFS {
   public:
	std::optional<std::vector<uint8_t>> readFile(
		const std::string& path) const override;
	bool exists(const std::string& path) const override;
};

}  // namespace bird::platform

#endif	// BIRD_VFS_H
