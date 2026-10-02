// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/android/resources/etc1_utils.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/bits.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/memory/aligned_memory.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "skia/ext/skia_utils_base.h"
// TODO(crbug.com/567908337): Remove //third_party/android_opengl/etc1 once the
// Rust ETC1 decoder launches.
#include "third_party/android_opengl/etc1/etc1.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMallocPixelRef.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/android/texture_compressor/cxx.rs.h"
#include "ui/android/ui_android_features.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace ui {

namespace {

constexpr uint32_t kCompressedKey = 0xABABABAB;
constexpr uint32_t kCurrentExtraVersion = 1;

// On-disk header layout:
//   [0..4)   kCompressedKey (uint32_t, big-endian)
//   [4..8)   content width  (uint32_t, big-endian)
//   [8..12)  content height (uint32_t, big-endian)
//   [12..28) ETC1 PKM header (ETC_PKM_HEADER_SIZE == 16 bytes)
constexpr size_t kHeaderSize = 3 * sizeof(uint32_t) + ETC_PKM_HEADER_SIZE;
static_assert(kHeaderSize == 28);

// On-disk trailer layout (kCurrentExtraVersion == 1):
//   [0..4)   extra_data_version (uint32_t, big-endian)
//   [4..8)   1.f / scale        (float, big-endian)
constexpr size_t kTrailerSize = sizeof(uint32_t) + sizeof(float);
static_assert(kTrailerSize == 8);

constexpr int kBlockSize = 4;

unsigned int NextPowerOfTwo(int a) {
  CHECK_GE(a, 0);
  return a == 0 ? 0u : std::bit_ceil(static_cast<uint32_t>(a));
}

unsigned int RoundUpMod4(int a) {
  CHECK_GE(a, 0);
  return base::bits::AlignUp(static_cast<uint32_t>(a), 4u);
}

// TODO(khushalsagar): This is a hack to ensure correct byte size computation
// for SkPixelRefs wrapping encoded data for ETC1 compressed bitmaps. We ideally
// shouldn't be using SkPixelRefs to wrap encoded data.
size_t ETC1RowBytes(int width) {
  DCHECK_EQ(width & 1, 0);
  return width / 2;
}

gfx::Size GetETCEncodedSize(const gfx::Size& bitmap_size, bool supports_npot) {
  CHECK_GE(bitmap_size.width(), 0);
  CHECK_GE(bitmap_size.height(), 0);
  DCHECK(!bitmap_size.IsEmpty());

  if (!supports_npot) {
    return gfx::Size(NextPowerOfTwo(RoundUpMod4(bitmap_size.width())),
                     NextPowerOfTwo(RoundUpMod4(bitmap_size.height())));
  } else {
    return gfx::Size(RoundUpMod4(bitmap_size.width()),
                     RoundUpMod4(bitmap_size.height()));
  }
}

// Check that `data` is sufficiently aligned for `T` and cast it to a Rust slice
// of `T`.
template <typename T>
rust::Slice<T> CastToAlignedSlice(void* data, size_t bytes) {
  CHECK(data);
  CHECK(base::IsAligned(data, alignof(T)));
  return {reinterpret_cast<T*>(data), bytes / sizeof(T)};
}

}  // namespace

// static
sk_sp<SkPixelRef> Etc1::CompressBitmap(const SkBitmap& raw_data,
                                       bool supports_etc_npot) {
  if (raw_data.empty() || !raw_data.getPixels() ||
      raw_data.bytesPerPixel() != sizeof(uint32_t) ||
      (raw_data.rowBytes() % sizeof(uint32_t)) != 0 ||
      !base::IsAligned(raw_data.getPixels(), alignof(uint32_t))) {
    return nullptr;
  }

  const gfx::Size raw_data_size(raw_data.width(), raw_data.height());
  const gfx::Size encoded_size =
      GetETCEncodedSize(raw_data_size, supports_etc_npot);

  size_t encoded_bytes =
      etc1_get_encoded_data_size(encoded_size.width(), encoded_size.height());
  SkImageInfo info =
      SkImageInfo::Make(encoded_size.width(), encoded_size.height(),
                        kUnknown_SkColorType, kUnpremul_SkAlphaType);
  const gfx::Size block_aligned_size(RoundUpMod4(raw_data_size.width()),
                                     RoundUpMod4(raw_data_size.height()));
  sk_sp<SkData> etc1_pixel_data =
      encoded_size == block_aligned_size
          ? SkData::MakeUninitialized(encoded_bytes)
          : SkData::MakeZeroInitialized(encoded_bytes);
  sk_sp<SkPixelRef> etc1_pixel_ref(SkMallocPixelRef::MakeWithData(
      info, ETC1RowBytes(encoded_size.width()), std::move(etc1_pixel_data)));

  compress_etc1(
      CastToAlignedSlice<const uint32_t>(raw_data.getPixels(),
                                         raw_data.computeByteSize()),
      CastToAlignedSlice<uint8_t>(etc1_pixel_ref->pixels(), encoded_bytes),
      raw_data.width(), raw_data.height(), raw_data.rowBytesAsPixels(),
      encoded_size.width() / kBlockSize);
  etc1_pixel_ref->setImmutable();
  return etc1_pixel_ref;
}

// static
SkBitmap Etc1::DecompressBitmap(const gfx::Size& content_size,
                                const sk_sp<SkPixelRef>& compressed_data) {
  if (content_size.IsEmpty() || !compressed_data ||
      !compressed_data->pixels()) {
    return SkBitmap();
  }

  const gfx::Size buffer_size(compressed_data->width(),
                              compressed_data->height());
  if ((buffer_size.width() % kBlockSize) != 0 ||
      (buffer_size.height() % kBlockSize) != 0 ||
      compressed_data->rowBytes() != ETC1RowBytes(buffer_size.width()) ||
      content_size.width() > buffer_size.width() ||
      content_size.height() > buffer_size.height()) {
    return SkBitmap();
  }

  if (base::FeatureList::IsEnabled(kUseNewEtc1Decoder)) {
    SkBitmap raw_data_small;
    raw_data_small.allocPixels(SkImageInfo::MakeN32(
        content_size.width(), content_size.height(), kOpaque_SkAlphaType));
    size_t encoded_bytes = 0;
    if (!base::CheckMul(compressed_data->rowBytes(),
                        static_cast<size_t>(buffer_size.height()))
             .AssignIfValid(&encoded_bytes)) {
      return SkBitmap();
    }
    decompress_etc1(
        CastToAlignedSlice<const uint8_t>(compressed_data->pixels(),
                                          encoded_bytes),
        CastToAlignedSlice<uint32_t>(raw_data_small.getPixels(),
                                     raw_data_small.computeByteSize()),
        content_size.width(), content_size.height(),
        buffer_size.width() / kBlockSize, raw_data_small.rowBytesAsPixels());
    raw_data_small.setImmutable();
    return raw_data_small;
  }

  // TODO(crbug.com/567908337): Remove the legacy C ETC1 decoder and encoder in
  // //third_party/android_opengl/etc1 once kUseNewEtc1Decoder launches.
  SkBitmap raw_data;
  raw_data.allocPixels(SkImageInfo::MakeN32(
      buffer_size.width(), buffer_size.height(), kOpaque_SkAlphaType));
  const bool success =
      etc1_decode_image(static_cast<const uint8_t*>(compressed_data->pixels()),
                        reinterpret_cast<unsigned char*>(raw_data.getPixels()),
                        buffer_size.width(), buffer_size.height(),
                        raw_data.bytesPerPixel(), raw_data.rowBytes());
  raw_data.setImmutable();
  if (!success) {
    return SkBitmap();
  }
  if (content_size == buffer_size) {
    return raw_data;
  }

  // The content size is smaller than the buffer size (likely because of
  // block alignment or power-of-two rounding), so deep copy the bitmap.
  SkBitmap raw_data_small;
  raw_data_small.allocPixels(SkImageInfo::MakeN32(
      content_size.width(), content_size.height(), kOpaque_SkAlphaType));
  SkCanvas small_canvas(raw_data_small);
  small_canvas.drawImage(raw_data.asImage(), 0, 0);
  raw_data_small.setImmutable();
  return raw_data_small;
}

bool Etc1::WriteToFile(base::File* file,
                       const gfx::Size& content_size,
                       float scale,
                       const sk_sp<SkPixelRef>& compressed_data) {
  CHECK(file);
  CHECK(compressed_data);
  if (!file->IsValid()) {
    return false;
  }

  CHECK_GE(compressed_data->width(), 0);
  CHECK_GE(compressed_data->height(), 0);
  unsigned width = static_cast<unsigned>(compressed_data->width());
  unsigned height = static_cast<unsigned>(compressed_data->height());

  // Pack the 28-byte header (key, width, height, and 16-byte PKM header) into a
  // single write syscall.
  std::array<uint8_t, kHeaderSize> header;
  base::SpanWriter<uint8_t> header_writer(header);
  header_writer.WriteU32BigEndian(kCompressedKey);
  header_writer.WriteU32BigEndian(
      base::checked_cast<uint32_t>(content_size.width()));
  header_writer.WriteU32BigEndian(
      base::checked_cast<uint32_t>(content_size.height()));
  etc1_pkm_format_header(header_writer.Skip<ETC_PKM_HEADER_SIZE>()->data(),
                         width, height);
  DCHECK_EQ(header_writer.remaining(), 0u);
  if (file->WriteAtCurrentPos(header) != header.size()) {
    return false;
  }

  const size_t data_size = etc1_get_encoded_data_size(width, height);
  // SAFETY: buffer interacts with external API.
  auto pixels = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const uint8_t*>(compressed_data->pixels()), data_size));
  if (file->WriteAtCurrentPos(pixels) != data_size) {
    return false;
  }

  // Pack the 8-byte trailer (version and inverted scale) into a single write
  // syscall.
  std::array<uint8_t, kTrailerSize> trailer;
  base::SpanWriter<uint8_t> trailer_writer(trailer);
  trailer_writer.WriteU32BigEndian(kCurrentExtraVersion);
  trailer_writer.Write(base::FloatToBigEndian(1.f / scale));
  DCHECK_EQ(trailer_writer.remaining(), 0u);
  if (file->WriteAtCurrentPos(trailer) != trailer.size()) {
    return false;
  }

  return true;
}

bool Etc1::ReadFromFile(base::File* file,
                        gfx::Size* out_content_size,
                        float* out_scale,
                        sk_sp<SkPixelRef>* out_pixels) {
  CHECK(file);
  if (!file->IsValid()) {
    return false;
  }

  std::array<uint8_t, kHeaderSize> header;
  if (file->ReadAtCurrentPos(header) != header.size()) {
    return false;
  }

  base::SpanReader<const uint8_t> header_reader(header);
  uint32_t key = 0;
  uint32_t raw_content_width = 0;
  uint32_t raw_content_height = 0;
  if (!header_reader.ReadU32BigEndian(key) || key != kCompressedKey ||
      !header_reader.ReadU32BigEndian(raw_content_width) ||
      raw_content_width == 0u ||
      !base::IsValueInRangeForNumericType<int>(raw_content_width) ||
      !header_reader.ReadU32BigEndian(raw_content_height) ||
      raw_content_height == 0u ||
      !base::IsValueInRangeForNumericType<int>(raw_content_height)) {
    return false;
  }

  const int content_width = base::checked_cast<int>(raw_content_width);
  const int content_height = base::checked_cast<int>(raw_content_height);
  out_content_size->SetSize(content_width, content_height);

  // Read ETC1 header.
  base::span<const uint8_t, ETC_PKM_HEADER_SIZE> etc1_buffer =
      *header_reader.Read<ETC_PKM_HEADER_SIZE>();
  if (!etc1_pkm_is_valid(etc1_buffer.data())) {
    return false;
  }

  const int raw_width = etc1_pkm_get_width(etc1_buffer.data());
  const int raw_height = etc1_pkm_get_height(etc1_buffer.data());
  if (raw_width <= 0 || raw_height <= 0 || (raw_width % 4) != 0 ||
      (raw_height % 4) != 0 || content_width > raw_width ||
      content_height > raw_height) {
    return false;
  }

  // Do some simple sanity check validation.  We can't have thumbnails larger
  // than the max display size of the screen.  We also can't have etc1 texture
  // data larger than the next power of 2 up from that.
  auto* screen = display::Screen::Get();
  if (!screen) {
    return false;
  }
  gfx::Size display_size = screen->GetPrimaryDisplay().GetSizeInPixel();
  const int max_dimension =
      std::max(display_size.width(), display_size.height());
  const size_t max_etc1_dimension = NextPowerOfTwo(max_dimension);

  if (content_width > max_dimension || content_height > max_dimension ||
      static_cast<size_t>(raw_width) > max_etc1_dimension ||
      static_cast<size_t>(raw_height) > max_etc1_dimension) {
    return false;
  }

  const int data_size = etc1_get_encoded_data_size(raw_width, raw_height);
  sk_sp<SkData> etc1_pixel_data(SkData::MakeUninitialized(data_size));

  std::optional<size_t> pixel_bytes_read =
      file->ReadAtCurrentPos(skia::as_writable_byte_span(*etc1_pixel_data));
  if (pixel_bytes_read != static_cast<size_t>(data_size)) {
    return false;
  }

  SkImageInfo info = SkImageInfo::Make(
      raw_width, raw_height, kUnknown_SkColorType, kUnpremul_SkAlphaType);

  *out_pixels = SkMallocPixelRef::MakeWithData(info, ETC1RowBytes(raw_width),
                                               std::move(etc1_pixel_data));
  (*out_pixels)->setImmutable();

  std::array<uint8_t, kTrailerSize> trailer;
  std::optional<size_t> trailer_bytes_read = file->ReadAtCurrentPos(trailer);
  if (!trailer_bytes_read || *trailer_bytes_read < 4u) {
    return false;
  }
  base::SpanReader<const uint8_t> trailer_reader(
      base::span(trailer).first(*trailer_bytes_read));
  uint32_t extra_data_version = 0;
  if (!trailer_reader.ReadU32BigEndian(extra_data_version)) {
    return false;
  }

  *out_scale = 1.f;
  if (extra_data_version == 1u) {
    auto scale_bytes = trailer_reader.Read<4>();
    if (!scale_bytes) {
      return false;
    }
    *out_scale = base::FloatFromBigEndian(*scale_bytes);
    if (!std::isfinite(*out_scale) || *out_scale <= 0.f) {
      return false;
    }
    *out_scale = 1.f / *out_scale;
    if (!std::isfinite(*out_scale)) {
      return false;
    }
  }

  return true;
}

}  // namespace ui
