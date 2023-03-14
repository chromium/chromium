// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"
#include "ui/base/clipboard/url_file_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::StringPiece data_piece(reinterpret_cast<const char*>(data), size);
  std::string url =
      ui::clipboard_util::internal::ExtractURLFromURLFileContents(data_piece);
  return 0;
}
