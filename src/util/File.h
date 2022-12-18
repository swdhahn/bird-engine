//
// Created by student on 12/8/22.
//

#ifndef BIRD_FILE_H
#define BIRD_FILE_H

#include <iostream>
#include <fstream>
#include <string>

namespace bird {

    //std::string readFile(std::string& path);
    std::unique_ptr<std::byte[]> readBinaryFile(std::string path, size_t& length);

}


#endif //BIRD_FILE_H
