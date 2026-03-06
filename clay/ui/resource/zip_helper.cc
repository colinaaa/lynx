// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "clay/ui/resource/zip_helper.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "clay/fml/file.h"
#include "clay/fml/logging.h"
#include "third_party/zlib/contrib/minizip/ioapi.h"
#include "third_party/zlib/contrib/minizip/unzip.h"
#include "third_party/zlib/contrib/minizip/zip.h"

namespace clay {

bool ZipHelper::Decompress(const std::string& zip_path,
                           const std::string& output_dir) {
  zlib_filefunc_def file_func;
  fill_fopen_filefunc(&file_func);
  unzFile zip_file = unzOpen2(zip_path.c_str(), &file_func);
  if (!zip_file) {
    FML_LOG(INFO) << "Failed to open ZIP file: " << zip_path;
    return false;
  }
  int ret = unzGoToFirstFile(zip_file);
  while (ret == UNZ_OK) {
    char filename_in_zip[512] = {0};
    unz_file_info file_info;
    unzGetCurrentFileInfo(zip_file, &file_info, filename_in_zip,
                          sizeof(filename_in_zip), nullptr, 0, nullptr, 0);
    size_t len = strlen(filename_in_zip);
    if (len > 0 && filename_in_zip[len - 1] == '/') {
      ret = unzGoToNextFile(zip_file);
      continue;
    }
    if (unzOpenCurrentFile(zip_file) != UNZ_OK) {
      FML_LOG(INFO) << "Failed to open file in ZIP: " << filename_in_zip;
      ret = unzGoToNextFile(zip_file);
      continue;
    }
    std::vector<uint8_t> file_data(file_info.uncompressed_size);
    int bytes_read = unzReadCurrentFile(zip_file, file_data.data(),
                                        file_info.uncompressed_size);
    unzCloseCurrentFile(zip_file);
    if (bytes_read < 0) {
      FML_LOG(INFO) << "Failed to read file: " << filename_in_zip;
      ret = unzGoToNextFile(zip_file);
      continue;
    }
    file_data.resize(bytes_read);
    std::string full_path = output_dir + "/" + filename_in_zip;
    std::filesystem::create_directories(
        std::filesystem::path(full_path).parent_path());
    std::ofstream ofs(full_path, std::ios::binary);
    if (!ofs) {
      FML_LOG(INFO) << "Failed to write file: " << full_path;
    } else {
      ofs.write(reinterpret_cast<const char*>(file_data.data()),
                file_data.size());
      ofs.close();
      FML_LOG(INFO) << "Extracted: " << full_path;
    }
    ret = unzGoToNextFile(zip_file);
  }
  unzClose(zip_file);
  return true;
}

bool ZipHelper::CompressTo(const std::string& zip_path,
                           const std::string& file_name_in_zip,
                           const uint8_t* data, size_t data_size,
                           bool compress) {
  zipFile zf = zipOpen(zip_path.c_str(), APPEND_STATUS_CREATE);
  if (zf == nullptr) {
    return false;
  }
  zip_fileinfo zi = {};
  int level = compress ? Z_DEFAULT_COMPRESSION : 0;
  int err = zipOpenNewFileInZip(zf, file_name_in_zip.c_str(), &zi, nullptr, 0,
                                nullptr, 0, nullptr, compress ? Z_DEFLATED : 0,
                                level);
  if (err != ZIP_OK) {
    zipClose(zf, nullptr);
    return false;
  }

  err = zipWriteInFileInZip(zf, data, static_cast<unsigned int>(data_size));
  if (err != ZIP_OK) {
    zipCloseFileInZip(zf);
    zipClose(zf, nullptr);
    return false;
  }
  zipCloseFileInZip(zf);
  zipClose(zf, nullptr);
  return true;
}

}  // namespace clay
