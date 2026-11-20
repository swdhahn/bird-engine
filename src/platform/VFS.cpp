#include "VFS.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace bird::platform {

namespace {

std::unique_ptr<VFS>& activeVfs() {
	static std::unique_ptr<VFS> vfs = std::make_unique<NativeVFS>();
	return vfs;
}

}  // namespace

const VFS& VFS::get() {
	return *activeVfs();
}

void VFS::set(std::unique_ptr<VFS> vfs) {
	if (!vfs) throw std::invalid_argument("VFS::set: null VFS");
	activeVfs() = std::move(vfs);
}

std::optional<std::vector<uint8_t>> NativeVFS::readFile(
	const std::string& path) const {
	std::ifstream file(path, std::ios::binary);
	if (!file) return std::nullopt;
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
							   std::istreambuf_iterator<char>());
	if (file.bad()) return std::nullopt;
	return bytes;
}

bool NativeVFS::exists(const std::string& path) const {
	std::error_code error;
	return std::filesystem::is_regular_file(path, error);
}

}  // namespace bird::platform
