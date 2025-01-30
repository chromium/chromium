// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ANDROID_RESOURCES_ETC1_UTILS_H_
#define UI_ANDROID_RESOURCES_ETC1_UTILS_H_

#include "base/feature_list.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/android/ui_android_export.h"

namespace base {
class File;
}  // namespace base

namespace gfx {
class Size;
}  // namespace gfx

class SkBitmap;
class SkPixelRef;

namespace ui {

UI_ANDROID_EXPORT BASE_DECLARE_FEATURE(kCompressBitmapAtBackgroundPriority);

class UI_ANDROID_EXPORT Etc1 {
  public:
  // Compresses `raw_data` using ETC1 compression into an SkPixelRef. Can be
  // called on any thread. Returns nullptr on failure.
  // The compressed bitmap can then be used to create a UIResource.
  static sk_sp<SkPixelRef> CompressBitmap(SkBitmap raw_data,
                                          bool supports_etc_npot);
  // Same as above, lowering the thread priority while compression is in
  // progress. Only use in cases where latency is not important.
  static sk_sp<SkPixelRef> CompressBitmapAtBackgroundPriority(
      SkBitmap raw_data,
      bool supports_etc_npot);

  static bool WriteToFile(base::File* file,
                                          const gfx::Size& content_size,
                                          const float scale,
                                          sk_sp<SkPixelRef> compressed_data);
  static bool ReadFromFile(base::File* file,
                                           gfx::Size* out_content_size,
                                           float* out_scale,
                                           sk_sp<SkPixelRef>* out_pixels);
};

}

#endif  // UI_ANDROID_RESOURCES_ETC1_UTILS_H_
