// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/etc1_utils.h"

#include "base/files/file.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/byte_conversions.h"
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/android/buildflags.h"
#include "ui/android/ui_android_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(UI_ANDROID_ENABLE_NEW_TEXTURE_COMPRESSOR)
#include "ui/android/texture_compressor/cxx.rs.h"
#endif

namespace ui {

// Used at callsites, to lower thread priority to background while compression
// is happening.
BASE_FEATURE(kCompressBitmapAtBackgroundPriority,
             "CompressBitmapAtBackgroundPriority",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

const uint32_t kCompressedKey = 0xABABABAB;
const uint32_t kCurrentExtraVersion = 1;

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

// TODO(khushalsagar): This is a hack to ensure correct byte size computation
// for SkPixelRefs wrapping encoded data for ETC1 compressed bitmaps. We ideally
// shouldn't be using SkPixelRefs to wrap encoded data.
size_t ETC1RowBytes(int width) {
  DCHECK_EQ(width & 1, 0);
  return width / 2;
}

bool WriteBigEndianU32ToFile(base::File* file,
                             base::StrictNumeric<uint32_t> v) {
  return file->WriteAtCurrentPos(base::U32ToBigEndian(v)) == sizeof(v);
}

bool WriteBigEndianFloatToFile(base::File* file, float v) {
  return file->WriteAtCurrentPos(base::FloatToBigEndian(v)) == sizeof(v);
}

bool ReadBigEndianU32FromFile(base::File* file, uint32_t* out) {
  std::array<uint8_t, sizeof(*out)> buffer;
  if (file->ReadAtCurrentPos(buffer).value_or(0u) != buffer.size()) {
    return false;
  }
  *out = base::U32FromBigEndian(buffer);
  return true;
}
bool ReadBigEndianFloatFromFile(base::File* file, float* out) {
  std::array<uint8_t, sizeof(*out)> buffer;
  if (file->ReadAtCurrentPos(buffer).value_or(0u) != buffer.size()) {
    return false;
  }
  *out = base::FloatFromBigEndian(buffer);
  return true;
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

#if BUILDFLAG(UI_ANDROID_ENABLE_NEW_TEXTURE_COMPRESSOR)
// Check that `data` is sufficiently aligned for `T` and cast it to a Rust slice
// of `T`.
template <typename T>
rust::Slice<T> CastToAlignedSlice(void* data, size_t bytes) {
  CHECK(base::IsAligned(data, alignof(T)));
  return {reinterpret_cast<T*>(data), bytes / sizeof(T)};
}
#endif

}  // namespace

// static
sk_sp<SkPixelRef> Etc1::CompressBitmap(SkBitmap raw_data,
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

#if BUILDFLAG(UI_ANDROID_ENABLE_NEW_TEXTURE_COMPRESSOR)
  constexpr int kBlockSize = 4;
  if (base::FeatureList::IsEnabled(kUseNewEtc1Encoder)) {
    // We assume the input slice is aligned to 4 bytes, which seems to hold in
    // practice.
    compress_etc1(CastToAlignedSlice<const uint32_t>(
                      raw_data.getPixels(), raw_data.computeByteSize()),
                  CastToAlignedSlice<unsigned char>(etc1_pixel_ref->pixels(),
                                                    encoded_bytes),
                  raw_data.width(), raw_data.height(),
                  raw_data.rowBytesAsPixels(),
                  encoded_size.width() / kBlockSize);
    return etc1_pixel_ref;
  }
#endif

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

sk_sp<SkPixelRef> Etc1::CompressBitmapAtBackgroundPriority(
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

bool Etc1::WriteToFile(base::File* file,
                       const gfx::Size& content_size,
                       const float scale,
                       sk_sp<SkPixelRef> compressed_data) {
  if (!file->IsValid()) {
    return false;
  }

  if (!WriteBigEndianU32ToFile(file, kCompressedKey)) {
    return false;
  }

  if (!WriteBigEndianU32ToFile(
          file, base::checked_cast<uint32_t>(content_size.width()))) {
    return false;
  }

  if (!WriteBigEndianU32ToFile(
          file, base::checked_cast<uint32_t>(content_size.height()))) {
    return false;
  }

  // Write ETC1 header.
  CHECK(compressed_data->width() >= 0);
  CHECK(compressed_data->height() >= 0);
  unsigned width = static_cast<unsigned>(compressed_data->width());
  unsigned height = static_cast<unsigned>(compressed_data->height());

  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  etc1_pkm_format_header(etc1_buffer, width, height);

  // SAFETY: buffer interacts with external API.
  int header_bytes_written = UNSAFE_BUFFERS(file->WriteAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer), ETC_PKM_HEADER_SIZE));
  if (header_bytes_written != ETC_PKM_HEADER_SIZE) {
    return false;
  }

  int data_size = etc1_get_encoded_data_size(width, height);
  // SAFETY: buffer interacts with external API.
  int pixel_bytes_written = UNSAFE_BUFFERS(file->WriteAtCurrentPos(
      reinterpret_cast<char*>(compressed_data->pixels()), data_size));
  if (pixel_bytes_written != data_size) {
    return false;
  }

  if (!WriteBigEndianU32ToFile(file, kCurrentExtraVersion)) {
    return false;
  }

  if (!WriteBigEndianFloatToFile(file, 1.f / scale)) {
    return false;
  }

  return true;
}

bool Etc1::ReadFromFile(base::File* file,
                  gfx::Size* out_content_size,
                  float* out_scale,
                  sk_sp<SkPixelRef>* out_pixels) {
  if (!file->IsValid()) {
    return false;
  }

  uint32_t key = 0;
  if (!ReadBigEndianU32FromFile(file, &key)) {
    return false;
  }

  if (key != kCompressedKey) {
    return false;
  }

  int content_width;
  {
    uint32_t val = 0;
    if (!ReadBigEndianU32FromFile(file, &val) || val == 0u ||
        !base::IsValueInRangeForNumericType<int>(val)) {
      return false;
    }
    content_width = base::checked_cast<int>(val);
  }

  int content_height;
  {
    uint32_t val = 0;
    if (!ReadBigEndianU32FromFile(file, &val) || val == 0u ||
        !base::IsValueInRangeForNumericType<int>(val)) {
      return false;
    }
    content_height = base::checked_cast<int>(val);
  }

  out_content_size->SetSize(content_width, content_height);

  // Read ETC1 header.
  int header_bytes_read = 0;
  unsigned char etc1_buffer[ETC_PKM_HEADER_SIZE];
  // SAFETY: buffer interacts with external API.
  header_bytes_read = UNSAFE_BUFFERS(file->ReadAtCurrentPos(
      reinterpret_cast<char*>(etc1_buffer), ETC_PKM_HEADER_SIZE));
  if (header_bytes_read != ETC_PKM_HEADER_SIZE) {
    return false;
  }

  if (!etc1_pkm_is_valid(etc1_buffer)) {
    return false;
  }

  int raw_width = 0;
  raw_width = etc1_pkm_get_width(etc1_buffer);
  if (raw_width <= 0) {
    return false;
  }

  int raw_height = 0;
  raw_height = etc1_pkm_get_height(etc1_buffer);
  if (raw_height <= 0) {
    return false;
  }

  // Do some simple sanity check validation.  We can't have thumbnails larger
  // than the max display size of the screen.  We also can't have etc1 texture
  // data larger than the next power of 2 up from that.
  gfx::Size display_size =
      display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel();
  int max_dimension = std::max(display_size.width(), display_size.height());

  if (content_width > max_dimension || content_height > max_dimension ||
      static_cast<size_t>(raw_width) > NextPowerOfTwo(max_dimension) ||
      static_cast<size_t>(raw_height) > NextPowerOfTwo(max_dimension)) {
    return false;
  }

  int data_size = etc1_get_encoded_data_size(raw_width, raw_height);
  sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(data_size));

  // SAFETY: buffer interacts with external API.
  int pixel_bytes_read = UNSAFE_BUFFERS(file->ReadAtCurrentPos(
    reinterpret_cast<char*>(etc1_pixel_data->writable_data()), data_size));

  if (pixel_bytes_read != data_size) {
    return false;
  }

  SkImageInfo info = SkImageInfo::Make(
      raw_width, raw_height, kUnknown_SkColorType, kUnpremul_SkAlphaType);

  *out_pixels = SkMallocPixelRef::MakeWithData(info, ETC1RowBytes(raw_width),
                                               std::move(etc1_pixel_data));

  uint32_t extra_data_version = 0;
  if (!ReadBigEndianU32FromFile(file, &extra_data_version)) {
    return false;
  }

  *out_scale = 1.f;
  if (extra_data_version == 1u) {
    if (!ReadBigEndianFloatFromFile(file, out_scale)) {
      return false;
    }

    if (*out_scale == 0.f) {
      return false;
    }

    *out_scale = 1.f / *out_scale;
  }

  return true;
}

}  // namespace ui
