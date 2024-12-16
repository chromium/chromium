// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/ui_resource_provider.h"

#include "base/feature_list.h"
#include "base/threading/platform_thread.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace ui {

// Used at callsites, to lower thread priority to background while compression
// is happening.
BASE_FEATURE(kCompressBitmapAtBackgroundPriority,
             "CompressBitmapAtBackgroundPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

unsigned int NextPowerOfTwo(int a) {
  DCHECK(a >= 0);
  auto x = static_cast<unsigned int>(a);
  --x;
  x |= x >> 1u;
  x |= x >> 2u;
  x |= x >> 4u;
  x |= x >> 8u;
  x |= x >> 16u;
  return x + 1;
}

unsigned int RoundUpMod4(int a) {
  DCHECK(a >= 0);
  auto x = static_cast<unsigned int>(a);
  return (x + 3u) & ~3u;
}

size_t ETC1RowBytes(int width) {
  DCHECK_EQ(width & 1, 0);
  return width / 2;
}

gfx::Size GetETCEncodedSize(const gfx::Size& bitmap_size, bool supports_npot) {
  DCHECK(bitmap_size.width() >= 0);
  DCHECK(bitmap_size.height() >= 0);
  DCHECK(!bitmap_size.IsEmpty());

  if (!supports_npot) {
    return gfx::Size(NextPowerOfTwo(bitmap_size.width()),
                     NextPowerOfTwo(bitmap_size.height()));
  } else {
    return gfx::Size(RoundUpMod4(bitmap_size.width()),
                     RoundUpMod4(bitmap_size.height()));
  }
}

}  // namespace

// static
sk_sp<SkPixelRef> UIResourceProvider::CompressBitmap(SkBitmap raw_data,
                                                     bool supports_etc_npot) {
  if (raw_data.empty()) {
    return nullptr;
  }

  const gfx::Size raw_data_size(raw_data.width(), raw_data.height());
  const gfx::Size encoded_size =
      GetETCEncodedSize(raw_data_size, supports_etc_npot);
  constexpr size_t kPixelSize = 4;  // For kARGB_8888_Config.
  size_t stride = kPixelSize * raw_data_size.width();

  size_t encoded_bytes =
      etc1_get_encoded_data_size(encoded_size.width(), encoded_size.height());
  SkImageInfo info =
      SkImageInfo::Make(encoded_size.width(), encoded_size.height(),
                        kUnknown_SkColorType, kUnpremul_SkAlphaType);
  sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(encoded_bytes));
  sk_sp<SkPixelRef> etc1_pixel_ref(SkMallocPixelRef::MakeWithData(
      info, ETC1RowBytes(encoded_size.width()), std::move(etc1_pixel_data)));

  if (etc1_encode_image(
          reinterpret_cast<unsigned char*>(raw_data.getPixels()),
          raw_data_size.width(), raw_data_size.height(), kPixelSize, stride,
          reinterpret_cast<unsigned char*>(etc1_pixel_ref->pixels()),
          encoded_size.width(), encoded_size.height())) {
    etc1_pixel_ref->setImmutable();
    return etc1_pixel_ref;
  }

  return nullptr;
}

sk_sp<SkPixelRef> UIResourceProvider::CompressBitmapAtBackgroundPriority(
    SkBitmap raw_data,
    bool supports_etc_npot) {
  // ETC1 compression (which happens below) is very expensive, taking 200-300ms
  // of a big core on high-end 2024 devices. As the thread priority is kept at
  // 120 (default) for thread pool threads, this is potentially competing with
  // more important threads, which either share the same priority, or are close
  // to it in importance.
  //
  // Temporarily lower the thread priority, to avoid competing with those. Note
  // that this does *not* restrict the thread to little cores only, as this is
  // directly going to the setpriority() system call, not through Android APIs
  // (which do not lower the thread priority either, as of Android 15 at least).
  base::ThreadType thread_type = base::PlatformThread::GetCurrentThreadType();
  base::PlatformThread::SetCurrentThreadType(base::ThreadType::kBackground);
  auto result = CompressBitmap(raw_data, supports_etc_npot);
  base::PlatformThread::SetCurrentThreadType(thread_type);

  return result;
}

}  // namespace ui
