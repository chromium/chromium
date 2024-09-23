// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_JAVA_BITMAP_H_
#define UI_GFX_ANDROID_JAVA_BITMAP_H_

#include <jni.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.gfx
// The order and values here match AndroidBitmapFormat, as verified
// by static_asserts in java_bitmap.cc.
enum BitmapFormat {
  BITMAP_FORMAT_NO_CONFIG = 0,
  BITMAP_FORMAT_ARGB_8888 = 1,
  BITMAP_FORMAT_RGB_565 = 4,
  BITMAP_FORMAT_ARGB_4444 = 7,
  BITMAP_FORMAT_ALPHA_8 = 8,
};

// This class wraps a JNI AndroidBitmap object to make it easier to use. It
// handles locking and unlocking of the underlying pixels, along with wrapping
// various JNI methods.
class GFX_EXPORT JavaBitmap {
 public:
  explicit JavaBitmap(const base::android::JavaRef<jobject>& bitmap);

  JavaBitmap(const JavaBitmap&) = delete;
  JavaBitmap& operator=(const JavaBitmap&) = delete;

  ~JavaBitmap();

  inline void* pixels() { return pixels_; }
  inline const void* pixels() const { return pixels_; }
  inline const gfx::Size& size() const { return size_; }
  inline BitmapFormat format() const { return format_; }
  inline uint32_t bytes_per_row() const { return bytes_per_row_; }
  inline int byte_count() const { return byte_count_; }

 private:
  base::android::ScopedJavaGlobalRef<jobject> bitmap_;
  raw_ptr<void> pixels_ = nullptr;
  gfx::Size size_;
  BitmapFormat format_;
  uint32_t bytes_per_row_;
  int byte_count_;
};

enum class OomBehavior {
  kCrashOnOom,
  kReturnNullOnOom,
};

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: |skbitmap| is assumed to be non-null, non-empty and one of RGBA_8888 or
// RGB_565 formats.
GFX_EXPORT base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap& skbitmap,
    OomBehavior reaction = OomBehavior::kCrashOnOom);

// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |jbitmap| is assumed to be non-null, non-empty and of format RGBA_8888.
GFX_EXPORT SkBitmap CreateSkBitmapFromJavaBitmap(const JavaBitmap& jbitmap);

// Returns a Skia color type value for the requested input java Bitmap.Config.
GFX_EXPORT SkColorType
ConvertToSkiaColorType(const base::android::JavaRef<jobject>& jbitmap_config);

}  // namespace gfx

namespace jni_zero {
// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |j_bitmap| is assumed to be non-null, non-empty and of format
// RGBA_8888.
template <>
GFX_EXPORT SkBitmap FromJniType<SkBitmap>(JNIEnv* env,
                                          const JavaRef<jobject>& j_bitmap);

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: return nullptr jobject if |skbitmap| is null or empty.
template <>
GFX_EXPORT ScopedJavaLocalRef<jobject> ToJniType<SkBitmap>(
    JNIEnv* env,
    const SkBitmap& skbitmap);
}  // namespace jni_zero

#endif  // UI_GFX_ANDROID_JAVA_BITMAP_H_
