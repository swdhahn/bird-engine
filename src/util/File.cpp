//
// Created by student on 12/8/22.
//

#include "File.h"

namespace bird {

std::string readFile(std::string path) {
  std::string text;
  std::ifstream file(path);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      text += line + "\n";
    }
    file.close();
  } else {
    std::cerr << "Could not open file: " << path << std::endl;
  }

  return text;
}

std::string readFileToString(std::string path) {
  std::string text;
  std::ifstream file(path);
  if (file.is_open()) {
    std::string line;
    while (std::getline(file, line)) {
      text += line + "\n";
    }
    file.close();
  } else {
    std::cerr << "Could not open file: " << path << std::endl;
  }

  return text;
}

std::unique_ptr<std::byte[]> readBinaryFile(std::string path, size_t &length) {
  std::streampos size;
  std::unique_ptr<std::byte[]> bytes;
  std::ifstream is(path, std::ios::in | std::ios::binary | std::ios::ate);
  if (is.is_open()) {
    size = is.tellg();
    bytes = std::make_unique<std::byte[]>(size);
    length = size;
    is.seekg(0, std::ios::beg);
    is.read((char *)bytes.get(), size);
    is.close();
  } else {
    std::cerr << "Could not read binary file: " << path << std::endl;
  }
  return bytes;
}

} // namespace bird
