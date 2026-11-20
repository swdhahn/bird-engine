#include "ApkAssetVFS.h"

#include <android/asset_manager.h>

#include <stdexcept>

namespace bird::platform {

namespace {

/** AAssetManager paths have no leading "./". */
std::string normalize(const std::string& path) {
	return path.rfind("./", 0) == 0 ? path.substr(2) : path;
}

}  // namespace

ApkAssetVFS::ApkAssetVFS(AAssetManager* manager) : m_manager(manager) {
	if (manager == nullptr) {
		throw std::invalid_argument("ApkAssetVFS: null asset manager");
	}
}

std::optional<std::vector<uint8_t>> ApkAssetVFS::readFile(
	const std::string& path) const {
	AAsset* asset = AAssetManager_open(m_manager, normalize(path).c_str(),
									   AASSET_MODE_BUFFER);
	if (asset == nullptr) return std::nullopt;
	const off64_t length = AAsset_getLength64(asset);
	std::vector<uint8_t> bytes(static_cast<size_t>(length));
	size_t offset = 0;
	while (offset < bytes.size()) {
		const int count = AAsset_read(asset, bytes.data() + offset,
									  bytes.size() - offset);
		if (count <= 0) break;
		offset += static_cast<size_t>(count);
	}
	AAsset_close(asset);
	if (offset != bytes.size()) return std::nullopt;
	return bytes;
}

bool ApkAssetVFS::exists(const std::string& path) const {
	AAsset* asset = AAssetManager_open(m_manager, normalize(path).c_str(),
									   AASSET_MODE_UNKNOWN);
	if (asset == nullptr) return false;
	AAsset_close(asset);
	return true;
}

}  // namespace bird::platform
