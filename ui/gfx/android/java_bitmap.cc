// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/java_bitmap.h"

#include <android/bitmap.h>

#include "base/android/jni_string.h"
#include "base/bits.h"
#include "base/check_op.h"
#include "base/debug/crash_logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/size.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/gfx/gfx_jni_headers/BitmapHelper_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace jni_zero {

// Converts |bitmap| to an SkBitmap of the same size and format.
// Note: |j_bitmap| is assumed to be non-null, non-empty and of format
// RGBA_8888.
template <>
SkBitmap FromJniType<SkBitmap>(JNIEnv* env, const JavaRef<jobject>& j_bitmap) {
  return gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_bitmap));
}

// Converts |skbitmap| to a Java-backed bitmap (android.graphics.Bitmap).
// Note: return nullptr jobject if |skbitmap| is null or empty.
template <>
ScopedJavaLocalRef<jobject> ToJniType<SkBitmap>(JNIEnv* env,
                                                const SkBitmap& skbitmap) {
  if (skbitmap.drawsNothing()) {
    return {};
  }
  return gfx::ConvertToJavaBitmap(skbitmap, gfx::OomBehavior::kCrashOnOom);
}
}  // namespace jni_zero

namespace gfx {
namespace {

int SkColorTypeToBitmapFormat(SkColorType color_type) {
  switch (color_type) {
    case kN32_SkColorType:
      return BITMAP_FORMAT_ARGB_8888;
    case kRGB_565_SkColorType:
      return BITMAP_FORMAT_RGB_565;
    default:
      // A bad format can cause out-of-bounds issues when copying pixels into or
      // out of the java bitmap's pixel buffer.
      CHECK_NE(color_type, color_type);
  }
  return BITMAP_FORMAT_NO_CONFIG;
}

SkColorType BitmapFormatToSkColorType(BitmapFormat bitmap_format) {
  switch (bitmap_format) {
    case BITMAP_FORMAT_ALPHA_8:
      return kAlpha_8_SkColorType;
    case BITMAP_FORMAT_ARGB_4444:
      return kARGB_4444_SkColorType;
    case BITMAP_FORMAT_ARGB_8888:
      return kN32_SkColorType;
    case BITMAP_FORMAT_RGB_565:
      return kRGB_565_SkColorType;
    case BITMAP_FORMAT_NO_CONFIG:
    default:
      SCOPED_CRASH_KEY_NUMBER("gfx", "bitmap_format",
                              static_cast<int>(bitmap_format));
      CHECK_NE(bitmap_format, bitmap_format);
      return kUnknown_SkColorType;
  }
}

// Wraps a Java bitmap as an SkPixmap. Since the pixmap references the
// underlying pixel data in the Java bitmap directly, it is only valid as long
// as |bitmap| is.
SkPixmap WrapJavaBitmapAsPixmap(const JavaBitmap& bitmap) {
  auto color_type = BitmapFormatToSkColorType(bitmap.format());
  auto image_info =
      SkImageInfo::Make(bitmap.size().width(), bitmap.size().height(),
                        color_type, kPremul_SkAlphaType);
  return SkPixmap(image_info, bitmap.pixels(), bitmap.bytes_per_row());
}

}  // namespace

#define ASSERT_ENUM_EQ(a, b) \
  static_assert(static_cast<int>(a) == static_cast<int>(b), "")

// BitmapFormat has the same values as AndroidBitmapFormat, for simplicitly, so
// that SkColorTypeToBitmapFormat() and the JavaBitmap::format() have the same
// values.
ASSERT_ENUM_EQ(BITMAP_FORMAT_NO_CONFIG, ANDROID_BITMAP_FORMAT_NONE);
ASSERT_ENUM_EQ(BITMAP_FORMAT_ALPHA_8, ANDROID_BITMAP_FORMAT_A_8);
ASSERT_ENUM_EQ(BITMAP_FORMAT_ARGB_4444, ANDROID_BITMAP_FORMAT_RGBA_4444);
ASSERT_ENUM_EQ(BITMAP_FORMAT_ARGB_8888, ANDROID_BITMAP_FORMAT_RGBA_8888);
ASSERT_ENUM_EQ(BITMAP_FORMAT_RGB_565, ANDROID_BITMAP_FORMAT_RGB_565);

JavaBitmap::JavaBitmap(const JavaRef<jobject>& bitmap) : bitmap_(bitmap) {
  int err = AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_.obj(),
                                     &pixels_.AsEphemeralRawAddr());
  DCHECK(!err);
  DCHECK(pixels_);

  AndroidBitmapInfo info;
  err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_.obj(), &info);
  DCHECK(!err);
  size_ = gfx::Size(info.width, info.height);
  format_ = static_cast<BitmapFormat>(info.format);
  bytes_per_row_ = info.stride;
  byte_count_ = Java_BitmapHelper_getByteCount(AttachCurrentThread(), bitmap_);
}

JavaBitmap::~JavaBitmap() {
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_.obj());
  DCHECK(!err);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap& skbitmap,
                                                OomBehavior reaction) {
  DCHECK(!skbitmap.isNull());
  DCHECK_GT(skbitmap.width(), 0);
  DCHECK_GT(skbitmap.height(), 0);

  int java_bitmap_format = SkColorTypeToBitmapFormat(skbitmap.colorType());

  ScopedJavaLocalRef<jobject> jbitmap = Java_BitmapHelper_createBitmap(
      AttachCurrentThread(), skbitmap.width(), skbitmap.height(),
      java_bitmap_format, reaction == OomBehavior::kReturnNullOnOom);
  if (!jbitmap) {
    DCHECK_EQ(OomBehavior::kReturnNullOnOom, reaction);
    return jbitmap;
  }

  JavaBitmap dst_lock(jbitmap);
  SkPixmap dst = WrapJavaBitmapAsPixmap(dst_lock);
  skbitmap.readPixels(dst);
  return jbitmap;
}

SkBitmap CreateSkBitmapFromJavaBitmap(const JavaBitmap& jbitmap) {
  DCHECK(!jbitmap.size().IsEmpty());
  DCHECK_GT(jbitmap.bytes_per_row(), 0U);
  DCHECK(jbitmap.pixels());

  // Ensure 4 byte stride alignment since the texture upload code in the
  // compositor relies on this.
  SkPixmap src = WrapJavaBitmapAsPixmap(jbitmap);
  const size_t min_row_bytes = src.info().minRowBytes();
  const size_t row_bytes = base::bits::AlignUp(min_row_bytes, size_t{4});

  SkBitmap skbitmap;
  skbitmap.allocPixels(src.info(), row_bytes);
  skbitmap.writePixels(src);
  return skbitmap;
}

SkColorType ConvertToSkiaColorType(const JavaRef<jobject>& bitmap_config) {
  BitmapFormat jbitmap_format =
      static_cast<BitmapFormat>(Java_BitmapHelper_getBitmapFormatForConfig(
          AttachCurrentThread(), bitmap_config));
  return BitmapFormatToSkColorType(jbitmap_format);
}

}  //  namespace gfx
