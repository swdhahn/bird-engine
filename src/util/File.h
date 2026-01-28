//
// Created by student on 12/8/22.
//

#ifndef BIRD_FILE_H
#define BIRD_FILE_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <fstream>
#include <iostream>
#include <memory.h>
#include <string>

namespace bird {

// std::string readFile(std::string& path);
std::unique_ptr<std::byte[]> readBinaryFile(std::string path, size_t &length);
std::string readFileToString(std::string path);

} // namespace bird

#endif // BIRD_FILE_H
