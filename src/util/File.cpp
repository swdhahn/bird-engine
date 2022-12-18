//
// Created by student on 12/8/22.
//

#include "File.h"

namespace bird {

    std::unique_ptr<std::byte[]> readBinaryFile(std::string path, size_t& length) {
        std::streampos size;
        std::unique_ptr<std::byte[]> bytes;
        std::ifstream is(path, std::ios::in | std::ios::binary | std::ios::ate);
        if(is.is_open()) {
            size = is.tellg();
            bytes = std::make_unique<std::byte[]>(size);
            length = size;
            is.seekg(0, std::ios::beg);
            is.read((char*)bytes.get(), size);
            is.close();
        } else {
            std::cerr << "" << std::endl;
        }
        return bytes;
    }

}
