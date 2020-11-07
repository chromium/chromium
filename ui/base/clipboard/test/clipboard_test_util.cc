// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/test/clipboard_test_util.h"

#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {

namespace clipboard_test_util {

namespace {

class ReadImageHelper {
 public:
  ReadImageHelper() = default;
  ~ReadImageHelper() = default;
  SkBitmap ReadImage(Clipboard* clipboard) {
    base::WaitableEvent event;
    SkBitmap bitmap;
    clipboard->ReadImage(
        ClipboardBuffer::kCopyPaste,
        /* data_dst = */ nullptr,
        base::BindLambdaForTesting([&](const SkBitmap& result) {
          bitmap = result;
          event.Signal();
        }));
    event.Wait();
    return bitmap;
  }
};

}  // namespace

SkBitmap ReadImage(Clipboard* clipboard) {
  ReadImageHelper read_image_helper;
  return read_image_helper.ReadImage(clipboard);
}

}  // namespace clipboard_test_util

}  // namespace ui
