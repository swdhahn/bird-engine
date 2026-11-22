#include "SaveGame.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
#include <pwd.h>
#include <unistd.h>
#endif

namespace bird {

namespace fs = std::filesystem;

namespace {

std::string getEnvVar(const char* name) {
	const char* val = std::getenv(name);
	return val ? std::string(val) : std::string();
}

void validateAppName(const std::string& appName) {
	if (appName.empty()) {
		throw std::invalid_argument("SaveGame: appName must not be empty");
	}
	if (appName == "." || appName == ".." ||
		appName.find('/') != std::string::npos ||
		appName.find('\\') != std::string::npos ||
		appName.find('\0') != std::string::npos) {
		throw std::invalid_argument(
			"SaveGame: appName contains invalid path characters: '" + appName +
			"'");
	}
}

fs::path validateRelativeFile(const std::string& relativeFile) {
	if (relativeFile.empty()) {
		throw std::invalid_argument("SaveGame: relativeFile must not be empty");
	}
	if (relativeFile.find('\0') != std::string::npos) {
		throw std::invalid_argument(
			"SaveGame: relativeFile contains null byte");
	}
	fs::path rel(relativeFile);
	if (rel.is_absolute()) {
		throw std::invalid_argument(
			"SaveGame: relativeFile must be relative, not absolute: '" +
			relativeFile + "'");
	}
	for (const auto& part : rel) {
		if (part == "..") {
			throw std::invalid_argument(
				"SaveGame: relativeFile must not contain '..' traversal: '" +
				relativeFile + "'");
		}
	}
	return rel;
}

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__) && !defined(__ANDROID__)
std::string resolvePosixHome() {
	std::string home = getEnvVar("HOME");
	if (!home.empty()) return home;
	if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) {
		return std::string(pw->pw_dir);
	}
	throw std::runtime_error(
		"SaveGame: unable to resolve HOME directory on POSIX");
}
#endif

bool writeBytesAtomically(const fs::path& targetPath, const char* data,
						  size_t size) {
	std::error_code ec;
	if (targetPath.has_parent_path()) {
		fs::create_directories(targetPath.parent_path(), ec);
		if (ec) return false;
	}

	fs::path tmpPath = targetPath;
	tmpPath += ".tmp";

	{
		std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) return false;
		if (size > 0) {
			out.write(data, static_cast<std::streamsize>(size));
		}
		out.flush();
		if (!out.good()) {
			out.close();
			fs::remove(tmpPath, ec);
			return false;
		}
	}

	fs::rename(tmpPath, targetPath, ec);
	if (ec) {
		// On Windows, rename fails if target already exists; remove and retry.
		std::error_code rmEc;
		fs::remove(targetPath, rmEc);
		ec.clear();
		fs::rename(tmpPath, targetPath, ec);
		if (ec) {
			fs::remove(tmpPath, rmEc);
			return false;
		}
	}
	return true;
}

}  // namespace

std::string SaveGame::getUserDataDirectory(const std::string& appName) {
	validateAppName(appName);

#if defined(_WIN32)
	std::string appData = getEnvVar("APPDATA");
	if (appData.empty()) {
		std::string userProfile = getEnvVar("USERPROFILE");
		if (userProfile.empty()) {
			throw std::runtime_error(
				"SaveGame: neither APPDATA nor USERPROFILE is set");
		}
		return (fs::path(userProfile) / "AppData" / "Roaming" / appName)
			.string();
	}
	return (fs::path(appData) / appName).string();

#elif defined(__APPLE__)
	const std::string home = resolvePosixHome();
	return (fs::path(home) / "Library" / "Application Support" / appName)
		.string();

#elif defined(__ANDROID__)
	return (fs::path("/data/data") / appName / "files").string();

#elif defined(__EMSCRIPTEN__)
	return (fs::path("/persistent") / appName).string();

#else
	// Linux / BSD / POSIX following the XDG Base Directory Specification.
	std::string xdgDataHome = getEnvVar("XDG_DATA_HOME");
	if (!xdgDataHome.empty() && fs::path(xdgDataHome).is_absolute()) {
		return (fs::path(xdgDataHome) / appName).string();
	}
	const std::string home = resolvePosixHome();
	return (fs::path(home) / ".local" / "share" / appName).string();
#endif
}

std::string SaveGame::getConfigDirectory(const std::string& appName) {
	validateAppName(appName);

#if defined(_WIN32) || defined(__APPLE__) || defined(__ANDROID__) || \
	defined(__EMSCRIPTEN__)
	return (fs::path(getUserDataDirectory(appName)) / "config").string();
#else
	std::string xdgConfigHome = getEnvVar("XDG_CONFIG_HOME");
	if (!xdgConfigHome.empty() && fs::path(xdgConfigHome).is_absolute()) {
		return (fs::path(xdgConfigHome) / appName).string();
	}
	const std::string home = resolvePosixHome();
	return (fs::path(home) / ".config" / appName).string();
#endif
}

std::string SaveGame::getSaveFilePath(const std::string& appName,
									  const std::string& relativeFile) {
	const fs::path base(getUserDataDirectory(appName));
	const fs::path rel = validateRelativeFile(relativeFile);
	return (base / rel).lexically_normal().string();
}

std::string SaveGame::getConfigFilePath(const std::string& appName,
										const std::string& relativeFile) {
	const fs::path base(getConfigDirectory(appName));
	const fs::path rel = validateRelativeFile(relativeFile);
	return (base / rel).lexically_normal().string();
}

bool SaveGame::ensureDirectoryExists(const std::string& dirPath) {
	if (dirPath.empty()) return false;
	std::error_code ec;
	if (fs::exists(dirPath, ec)) {
		return fs::is_directory(dirPath, ec);
	}
	return fs::create_directories(dirPath, ec) && !ec;
}

bool SaveGame::writeTextFile(const std::string& appName,
							 const std::string& relativeFile,
							 const std::string& content) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	return writeBytesAtomically(path, content.data(), content.size());
}

std::optional<std::string> SaveGame::tryReadTextFile(
	const std::string& appName, const std::string& relativeFile) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	std::ifstream in(path, std::ios::binary);
	if (!in.is_open()) return std::nullopt;
	std::ostringstream oss;
	oss << in.rdbuf();
	if (!in.good() && !in.eof()) return std::nullopt;
	return oss.str();
}

std::string SaveGame::readTextFile(const std::string& appName,
								   const std::string& relativeFile) {
	auto result = tryReadTextFile(appName, relativeFile);
	return result ? *result : std::string();
}

bool SaveGame::writeBinaryFile(const std::string& appName,
							   const std::string& relativeFile,
							   const std::vector<uint8_t>& bytes) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	return writeBytesAtomically(
		path, reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::optional<std::vector<uint8_t>> SaveGame::readBinaryFile(
	const std::string& appName, const std::string& relativeFile) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	if (!in.is_open()) return std::nullopt;
	const std::streamsize size = in.tellg();
	if (size < 0) return std::nullopt;
	in.seekg(0, std::ios::beg);
	std::vector<uint8_t> buf(static_cast<size_t>(size));
	if (size > 0 &&
		!in.read(reinterpret_cast<char*>(buf.data()), size)) {
		return std::nullopt;
	}
	return buf;
}

bool SaveGame::fileExists(const std::string& appName,
						  const std::string& relativeFile) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	std::error_code ec;
	return fs::is_regular_file(path, ec);
}

bool SaveGame::deleteFile(const std::string& appName,
						  const std::string& relativeFile) {
	const fs::path path(getSaveFilePath(appName, relativeFile));
	std::error_code ec;
	return fs::remove(path, ec) && !ec;
}

}  // namespace bird
