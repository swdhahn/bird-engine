//
// Created by Seth Hahn on 2/18/24.
//

#include <iostream>
#include <string>

#include "AssetLoader.h"

namespace {

void printUsage(const char* argv0) {
	std::cerr << "Usage: " << argv0
			  << " [--file <asset-path> | --dir <assets-dir>] [--force]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
	std::string singleFile;
	std::string directory = "assets";
	bool force = false;

	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--file") {
			if (i + 1 >= argc) {
				std::cerr << "Missing path argument for --file" << std::endl;
				printUsage(argv[0]);
				return 1;
			}
			singleFile = argv[++i];
		} else if (arg == "--dir") {
			if (i + 1 >= argc) {
				std::cerr << "Missing path argument for --dir" << std::endl;
				printUsage(argv[0]);
				return 1;
			}
			directory = argv[++i];
		} else if (arg == "--force") {
			force = true;
		} else if (arg == "--help" || arg == "-h") {
			printUsage(argv[0]);
			return 0;
		} else {
			std::cerr << "Unknown argument: " << arg << std::endl;
			printUsage(argv[0]);
			return 1;
		}
	}

	bird::AssetLoader loader;

	if (!singleFile.empty()) {
		if (!loader.compileFile(singleFile, force)) {
			std::cerr << "Asset failed to compile: " << singleFile << std::endl;
			return 1;
		}
		return 0;
	}

	const size_t failures = loader.init(directory, force);
	if (failures > 0) {
		std::cerr << failures << " asset(s) failed to compile" << std::endl;
		return 1;
	}
	return 0;
}
