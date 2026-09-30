// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANDROID_JAVA_BITMAP_H_
#define UI_GFX_ANDROID_JAVA_BITMAP_H_

#include <jni.h>

#include <cstdint>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

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
class COMPONENT_EXPORT(GFX) JavaBitmap {
 public:
  explicit JavaBitmap(const base::android::JavaRef<jobject>& bitmap);

  JavaBitmap(const JavaBitmap&) = delete;
  JavaBitmap& operator=(const JavaBitmap&) = delete;

  ~JavaBitmap();

  void* pixels() { return pixels_; }
  const void* pixels() const { return pixels_; }
  const gfx::Size& size() const { return size_; }
  BitmapFormat format() const { return format_; }
  uint32_t bytes_per_row() const { return bytes_per_row_; }
  int byte_count() const {
    return base::CheckMul<int>(bytes_per_row_, size_.height()).ValueOrDie();
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> bitmap_;
  raw_ptr<void> pixels_ = nullptr;
  gfx::Size size_;
  BitmapFormat format_ = BITMAP_FORMAT_NO_CONFIG;
  uint32_t bytes_per_row_ = 0;
};

enum class OomBehavior {
  kCrashOnOom,
  kReturnNullOnOom,
};

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: |skbitmap| is assumed to be non-null, non-empty and one of RGBA_8888 or
// RGB_565 formats.
COMPONENT_EXPORT(GFX)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(
    const SkBitmap& skbitmap,
    OomBehavior reaction = OomBehavior::kCrashOnOom);

// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |jbitmap| is assumed to be non-null, non-empty and of format RGBA_8888.
COMPONENT_EXPORT(GFX)
SkBitmap CreateSkBitmapFromJavaBitmap(const JavaBitmap& jbitmap);

}  // namespace gfx

namespace jni_zero {
// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |j_bitmap| is assumed to be non-null, non-empty and of format
// RGBA_8888.
template <>
COMPONENT_EXPORT(GFX)
SkBitmap FromJniType<SkBitmap>(JNIEnv* env, const JavaRef<jobject>& j_bitmap);

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: return nullptr jobject if |skbitmap| is null or empty.
template <>
COMPONENT_EXPORT(GFX)
ScopedJavaLocalRef<jobject> ToJniType<SkBitmap>(JNIEnv* env,
                                                const SkBitmap& skbitmap);
}  // namespace jni_zero

#endif  // UI_GFX_ANDROID_JAVA_BITMAP_H_
