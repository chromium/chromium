// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/android/java_bitmap.h"

#include <android/bitmap.h>

#include "base/android/jni_string.h"
#include "base/bits.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_jni_headers/BitmapHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::JavaRef;

namespace gfx {

JavaBitmap::JavaBitmap(const JavaRef<jobject>& bitmap)
    : bitmap_(bitmap), pixels_(NULL) {
  int err =
      AndroidBitmap_lockPixels(AttachCurrentThread(), bitmap_.obj(), &pixels_);
  DCHECK(!err);
  DCHECK(pixels_);

  AndroidBitmapInfo info;
  err = AndroidBitmap_getInfo(AttachCurrentThread(), bitmap_.obj(), &info);
  DCHECK(!err);
  size_ = gfx::Size(info.width, info.height);
  format_ = info.format;
  stride_ = info.stride;
  byte_count_ = Java_BitmapHelper_getByteCount(AttachCurrentThread(), bitmap_);
}

JavaBitmap::~JavaBitmap() {
  int err = AndroidBitmap_unlockPixels(AttachCurrentThread(), bitmap_.obj());
  DCHECK(!err);
}

static int SkColorTypeToBitmapFormat(SkColorType color_type) {
  switch (color_type) {
    case kAlpha_8_SkColorType:
      return BITMAP_FORMAT_ALPHA_8;
    case kARGB_4444_SkColorType:
      return BITMAP_FORMAT_ARGB_4444;
    case kN32_SkColorType:
      return BITMAP_FORMAT_ARGB_8888;
    case kRGB_565_SkColorType:
      return BITMAP_FORMAT_RGB_565;
    case kUnknown_SkColorType:
    default:
      NOTREACHED();
      return BITMAP_FORMAT_NO_CONFIG;
  }
}

ScopedJavaLocalRef<jobject> CreateJavaBitmap(int width,
                                             int height,
                                             SkColorType color_type,
                                             OomBehavior reaction) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  int java_bitmap_config = SkColorTypeToBitmapFormat(color_type);
  return Java_BitmapHelper_createBitmap(
      AttachCurrentThread(), width, height, java_bitmap_config,
      reaction == OomBehavior::kReturnNullOnOom);
}

ScopedJavaLocalRef<jobject> ConvertToJavaBitmap(const SkBitmap* skbitmap,
                                                OomBehavior reaction) {
  DCHECK(skbitmap);
  DCHECK(!skbitmap->isNull());
  SkColorType color_type = skbitmap->colorType();
  DCHECK((color_type == kRGB_565_SkColorType) ||
         (color_type == kN32_SkColorType));
  ScopedJavaLocalRef<jobject> jbitmap = CreateJavaBitmap(
      skbitmap->width(), skbitmap->height(), color_type, reaction);
  if (!jbitmap) {
    DCHECK_EQ(OomBehavior::kReturnNullOnOom, reaction);
    return jbitmap;
  }
  JavaBitmap dst_lock(jbitmap);
  void* src_pixels = skbitmap->getPixels();
  void* dst_pixels = dst_lock.pixels();
  CHECK_GE(base::checked_cast<size_t>(dst_lock.byte_count()),
           skbitmap->computeByteSize());
  memcpy(dst_pixels, src_pixels, skbitmap->computeByteSize());

  return jbitmap;
}

SkBitmap CreateSkBitmapFromJavaBitmap(const JavaBitmap& jbitmap) {
  DCHECK(!jbitmap.size().IsEmpty());
  DCHECK_GT(jbitmap.stride(), 0U);
  DCHECK(jbitmap.pixels());

  gfx::Size src_size = jbitmap.size();

  SkBitmap skbitmap;
  SkImageInfo image_info;
  switch (jbitmap.format()) {
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      image_info =
          SkImageInfo::MakeN32Premul(src_size.width(), src_size.height());
      break;
    case ANDROID_BITMAP_FORMAT_A_8:
      image_info =
          SkImageInfo::SkImageInfo::MakeA8(src_size.width(), src_size.height());
      break;
    default:
      CHECK(false) << "Invalid Java bitmap format: " << jbitmap.format();
      break;
  }

  // Ensure 4 byte stride alignment since the texture upload code in the
  // compositor relies on this.
  const size_t min_row_bytes = image_info.minRowBytes();
  DCHECK_GE(jbitmap.stride(), min_row_bytes);

  const size_t row_bytes = base::bits::Align(min_row_bytes, 4u);
  skbitmap.allocPixels(image_info, row_bytes);

  const char* src_pixels = static_cast<const char*>(jbitmap.pixels());
  char* dst_pixels = static_cast<char*>(skbitmap.getPixels());
  for (int i = 0; i < src_size.height(); ++i) {
    memcpy(dst_pixels, src_pixels, min_row_bytes);
    src_pixels += jbitmap.stride();
    dst_pixels += row_bytes;
  }
  return skbitmap;
}

SkColorType ConvertToSkiaColorType(const JavaRef<jobject>& bitmap_config) {
  int jbitmap_config = Java_BitmapHelper_getBitmapFormatForConfig(
      AttachCurrentThread(), bitmap_config);
  switch (jbitmap_config) {
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
      return kUnknown_SkColorType;
  }
}

}  //  namespace gfx
