// sherpa-onnx/csrc/file-utils.cc
//
// Copyright (c)  2022-2023  Xiaomi Corporation

#include "sherpa-onnx/csrc/file-utils.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include "sherpa-onnx/csrc/macros.h"

namespace sherpa_onnx {

bool FileExists(const std::string &filename) {
  return std::ifstream(filename).good();
}

void AssertFileExists(const std::string &filename) {
  if (!FileExists(filename)) {
    SHERPA_ONNX_LOGE("filename '%s' does not exist", filename.c_str());
    exit(-1);
  }
}



std::vector<char> ReadFile(const std::string& filename) {
    std::vector<char> buffer;

#ifdef _WIN32
    // Windows implementation using CreateFileMapping
    HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize) || fileSize.QuadPart < 0) {
        CloseHandle(hFile);
        throw std::runtime_error("Invalid file size: " + filename);
    }
    size_t size = static_cast<size_t>(fileSize.QuadPart);

    if (size > 0) {
        HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMapping) {
            CloseHandle(hFile);
            throw std::runtime_error("Failed to create file mapping: " + filename);
        }

        void* mapped = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
        if (!mapped) {
            CloseHandle(hMapping);
            CloseHandle(hFile);
            throw std::runtime_error("Failed to map view of file: " + filename);
        }

        // Copy to vector
        buffer.assign(static_cast<char*>(mapped),
                     static_cast<char*>(mapped) + size);

        // Cleanup
        UnmapViewOfFile(mapped);
        CloseHandle(hMapping);
    }
    CloseHandle(hFile);

#else
    // Unix-like (Mac/Linux/Android) implementation using mmap
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw std::runtime_error("Failed to get file size: " + filename);
    }
    size_t size = static_cast<size_t>(st.st_size);

    if (size > 0) {
        void* mapped = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Failed to map file: " + filename);
        }

        // Copy to vector
        buffer.assign(static_cast<char*>(mapped),
                     static_cast<char*>(mapped) + size);

        // Cleanup
        munmap(mapped, size);
    }
    close(fd);
#endif

    return buffer;
}

#if __ANDROID_API__ >= 9
std::vector<char> ReadFile(AAssetManager *mgr, const std::string &filename) {
  AAsset *asset = AAssetManager_open(mgr, filename.c_str(), AASSET_MODE_BUFFER);
  if (!asset) {
    __android_log_print(ANDROID_LOG_FATAL, "sherpa-onnx",
                        "Read binary file: Load %s failed", filename.c_str());
    exit(-1);
  }

  auto p = reinterpret_cast<const char *>(AAsset_getBuffer(asset));
  size_t asset_length = AAsset_getLength(asset);

  std::vector<char> buffer(p, p + asset_length);
  AAsset_close(asset);

  return buffer;
}
#endif

#if __OHOS__
std::vector<char> ReadFile(NativeResourceManager *mgr,
                           const std::string &filename) {
  std::unique_ptr<RawFile, decltype(&OH_ResourceManager_CloseRawFile)> fp(
      OH_ResourceManager_OpenRawFile(mgr, filename.c_str()),
      OH_ResourceManager_CloseRawFile);

  if (!fp) {
    std::ostringstream os;
    os << "Read file '" << filename << "' failed.";
    SHERPA_ONNX_LOGE("%s", os.str().c_str());
    return {};
  }

  auto len = static_cast<int32_t>(OH_ResourceManager_GetRawFileSize(fp.get()));

  std::vector<char> buffer(len);

  int32_t n = OH_ResourceManager_ReadRawFile(fp.get(), buffer.data(), len);

  if (n != len) {
    std::ostringstream os;
    os << "Read file '" << filename << "' failed. Number of bytes read: " << n
       << ". Expected bytes to read: " << len;
    SHERPA_ONNX_LOGE("%s", os.str().c_str());
    return {};
  }

  return buffer;
}
#endif

}  // namespace sherpa_onnx
