//
// Created by student on 12/8/22.
//

#ifndef BIRD_FILE_H
#define BIRD_FILE_H

#include <bits/stdc++.h>
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
