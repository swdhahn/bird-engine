//
// Created by Seth Hahn on 2/18/24.
//

#include <iostream>

#include "AssetLoader.h"

int main(int argc, char* argv[]) {
	bird::AssetLoader loader;

	const size_t failures = loader.init("assets");
	if (failures > 0) {
		std::cerr << failures << " asset(s) failed to compile" << std::endl;
		return 1;
	}
	return 0;
}
