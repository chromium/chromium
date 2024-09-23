// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/file_info.h"

#include <string_view>

namespace ui {

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view data_piece(reinterpret_cast<const char*>(data), size);
  std::vector<ui::FileInfo> files = ui::URIListToFileInfos(data_piece);
  return 0;
}

}  // namespace ui
