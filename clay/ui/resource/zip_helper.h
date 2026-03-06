// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef CLAY_UI_RESOURCE_ZIP_HELPER_H_
#define CLAY_UI_RESOURCE_ZIP_HELPER_H_

#include <string>

namespace clay {

class ZipHelper {
 public:
  static constexpr const char* kZip = ".zip";
  static bool Decompress(const std::string& zip_path,
                         const std::string& output_dir);
  static bool CompressTo(const std::string& zip_path,
                         const std::string& file_name_in_zip,
                         const uint8_t* data, size_t data_size, bool compress);
};

}  // namespace clay

#endif  // CLAY_UI_RESOURCE_ZIP_HELPER_H_
