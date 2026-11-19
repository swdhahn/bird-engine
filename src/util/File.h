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
#include <cstddef>

namespace bird {

/**
 * Reads an entire binary file into an allocated byte buffer.
 *
 * @param path Filesystem path to the file.
 * @param[out] length Set to the total size in bytes read from the file.
 * @return Unique pointer to the raw byte buffer, or nullptr on failure.
 */
std::unique_ptr<std::byte[]> readBinaryFile(std::string path, size_t &length);

/**
 * Reads an entire text file into a std::string.
 *
 * @param path Filesystem path to the text file.
 * @return String containing file contents, or empty string on failure.
 */
std::string readFileToString(std::string path);

} // namespace bird

#endif // BIRD_FILE_H
