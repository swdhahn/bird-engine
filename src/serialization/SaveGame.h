#ifndef BIRD_SERIALIZATION_SAVE_GAME_H
#define BIRD_SERIALIZATION_SAVE_GAME_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../util/Constants.h"

namespace bird {

/**
 * Cross-platform helper for resolving OS-appropriate user data and config
 * directories and safely reading/writing player save games outside the
 * application install directory.
 *
 * Resolved base directories:
 * - **Windows**: `%APPDATA%\<appName>` (config under `%APPDATA%\<appName>\config`)
 * - **macOS**: `~/Library/Application Support/<appName>`
 * - **Linux / POSIX**: `$XDG_DATA_HOME/<appName>` or `~/.local/share/<appName>`
 *   (config under `$XDG_CONFIG_HOME/<appName>` or `~/.config/<appName>`)
 *
 * All relative path helpers reject empty application names, absolute paths,
 * and `..` path-traversal segments so save data cannot escape its sandboxed
 * application folder. Writes use an atomic temp-file-and-rename pattern to
 * prevent save corruption if the process terminates mid-write.
 */
class SaveGame {
   public:
	/**
	 * Resolves the OS-appropriate user data directory for `appName`.
	 * Does not create the directory on disk; call ensureDirectoryExists() or
	 * writeTextFile() to create it lazily.
	 *
	 * @param appName Application/game identifier (must not contain path
	 *                separators or "..").
	 * @return Absolute directory path for user save data.
	 * @throws std::invalid_argument if `appName` is empty or invalid.
	 * @throws std::runtime_error if the OS user directory cannot be resolved.
	 */
	static std::string getUserDataDirectory(const std::string& appName);

	/**
	 * Resolves the OS-appropriate user configuration directory for `appName`.
	 *
	 * @param appName Application/game identifier.
	 * @return Absolute directory path for user configuration files.
	 * @throws std::invalid_argument if `appName` is empty or invalid.
	 * @throws std::runtime_error if the OS config directory cannot be resolved.
	 */
	static std::string getConfigDirectory(const std::string& appName);

	/**
	 * Resolves a safe absolute path to a save file inside the user data
	 * directory for `appName`.
	 *
	 * @param appName Application/game identifier.
	 * @param relativeFile Relative file path (e.g. `"slot1.json"` or `"saves/slot1.json"`).
	 * @return Full filesystem path inside getUserDataDirectory(appName).
	 * @throws std::invalid_argument if `appName` or `relativeFile` is empty,
	 *         absolute, or contains `".."` traversal components.
	 */
	static std::string getSaveFilePath(const std::string& appName,
									   const std::string& relativeFile);

	/**
	 * Resolves a safe absolute path to a config file inside the user config
	 * directory for `appName`.
	 *
	 * @param appName Application/game identifier.
	 * @param relativeFile Relative file path (e.g. `"settings.json"`).
	 * @return Full filesystem path inside getConfigDirectory(appName).
	 * @throws std::invalid_argument on invalid `appName` or `relativeFile`.
	 */
	static std::string getConfigFilePath(const std::string& appName,
										 const std::string& relativeFile);

	/**
	 * Ensures `dirPath` (and any missing parent directories) exist on disk.
	 * @return True if the directory exists or was created.
	 */
	static bool ensureDirectoryExists(const std::string& dirPath);

	/**
	 * Atomically writes UTF-8 text (e.g. JSON save data or scene state) to
	 * `relativeFile` inside the user data directory for `appName`.
	 * Creates parent directories automatically.
	 *
	 * @param appName Application/game identifier.
	 * @param relativeFile Relative save file path.
	 * @param content Text payload to write.
	 * @return True if written and atomically committed to disk.
	 */
	static bool writeTextFile(const std::string& appName,
							  const std::string& relativeFile,
							  const std::string& content);

	/**
	 * Reads a UTF-8 text save file from the user data directory for `appName`.
	 * @return File contents, or empty string if missing/unreadable.
	 */
	static std::string readTextFile(const std::string& appName,
									const std::string& relativeFile);

	/**
	 * Reads a UTF-8 text save file, distinguishing missing files via std::optional.
	 */
	BIRD_NO_EXPORT static std::optional<std::string> tryReadTextFile(
		const std::string& appName, const std::string& relativeFile);

	/**
	 * Atomically writes raw binary bytes to `relativeFile` inside the user data
	 * directory for `appName`.
	 */
	BIRD_NO_EXPORT static bool writeBinaryFile(
		const std::string& appName, const std::string& relativeFile,
		const std::vector<uint8_t>& bytes);

	/**
	 * Reads raw binary bytes from `relativeFile` inside the user data directory.
	 */
	BIRD_NO_EXPORT static std::optional<std::vector<uint8_t>> readBinaryFile(
		const std::string& appName, const std::string& relativeFile);

	/** Returns true if `relativeFile` exists in `appName`'s user data directory. */
	static bool fileExists(const std::string& appName,
						   const std::string& relativeFile);

	/** Deletes `relativeFile` from `appName`'s user data directory if it exists. */
	static bool deleteFile(const std::string& appName,
						   const std::string& relativeFile);
};

}  // namespace bird

#endif	// BIRD_SERIALIZATION_SAVE_GAME_H
