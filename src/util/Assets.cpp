//
// Created by student on 2/13/24.
//

#include "Assets.h"
#include "generic/Mesh.h"
#include "generic/Buffer.h"


namespace bird {

    namespace Assets {




        void loadSound(std::string& path);
        void loadShader(std::string& path);
        void loadCustom(std::string& path);
    }

    void AssetsLoader::init() {
        // load assets from file
    }

    void AssetsLoader::writeAssets() {
        // write assets to file
    }

    void AssetsLoader::loadAsset() {
        // load asset
    }

    void AssetsLoader::loadAssetMap() {
        // load asset
    }

    void AssetsLoader::createAssetMap() {
        // create asset map
        std::ofstream file("assets.map", std::ios::binary);

        file << "ASSETS";

        file.close();
    }

    void AssetsLoader::updateAssetMap() {

    }

}
