// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ui::clipboard_util {

// Encodes a bitmap to a PNG. This is an expensive method that must be
// called on a sequence that allows long-running CPU operations.
[[nodiscard]] COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
    std::vector<uint8_t> EncodeBitmapToPng(const SkBitmap& bitmap);

// Prefer EncodeBitmapToPng() if possible. Use this method only when encoding
// must be done synchronously from a sequence which does not allow long-running
// CPU operations, such as while writing to the clipboard from the UI thread,
// which may cause jank.
[[nodiscard]] COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
    std::vector<uint8_t> EncodeBitmapToPngAcceptJank(const SkBitmap& bitmap);

struct BookmarkData {
  std::u16string title;
  std::string url;
};

bool ShouldSkipBookmark(const std::u16string& title, const std::string& url);

}  // namespace ui::clipboard_util

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_UTIL_H_
