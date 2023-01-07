// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/test/clipboard_test_util.h"

#include <vector>

#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

namespace clipboard_test_util {

namespace {

class ReadImageHelper {
 public:
  ReadImageHelper() = default;
  ~ReadImageHelper() = default;

  std::vector<uint8_t> ReadPng(Clipboard* clipboard) {
    base::RunLoop loop;
    std::vector<uint8_t> png;
    clipboard->ReadPng(
        ClipboardBuffer::kCopyPaste,
        /* data_dst = */ nullptr,
        base::BindLambdaForTesting([&](const std::vector<uint8_t>& result) {
          png = result;
          loop.Quit();
        }));
    loop.Run();
    return png;
  }
};

}  // namespace

std::vector<uint8_t> ReadPng(Clipboard* clipboard) {
  ReadImageHelper read_image_helper;
  return read_image_helper.ReadPng(clipboard);
}

}  // namespace clipboard_test_util

}  // namespace ui
