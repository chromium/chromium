// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_span_util.h"

#include <limits>

#include "base/compiler_specific.h"
#include "skia/ext/skia_utils_base.h"

namespace gfx {

base::span<const uint8_t> SkPixmapToSpan(const SkPixmap& pixmap) {
  const size_t size_in_bytes = pixmap.computeByteSize();
  if (size_in_bytes == std::numeric_limits<size_t>::max()) {
    return {};
  }
  // SAFETY: SkPixmap's pointer is guaranteed to point to at least
  // `SkPixmap::computeByteSize()` many bytes.
  return UNSAFE_BUFFERS(
      base::span(static_cast<const uint8_t*>(pixmap.addr()), size_in_bytes));
}

base::span<uint8_t> SkPixmapToWritableSpan(const SkPixmap& pixmap) {
  const size_t size_in_bytes = pixmap.computeByteSize();
  if (size_in_bytes == std::numeric_limits<size_t>::max()) {
    return {};
  }
  // SAFETY: SkPixmap's pointer is guaranteed to point to at least
  // `SkPixmap::computeByteSize()` many bytes.
  return UNSAFE_BUFFERS(
      base::span(static_cast<uint8_t*>(pixmap.writable_addr()), size_in_bytes));
}

base::span<const uint8_t> SkDataToSpan(sk_sp<SkData> data) {
  if (!data) {
    return {};
  }
  return skia::as_byte_span(*data);
}

sk_sp<SkData> MakeSkDataFromSpanWithCopy(base::span<const uint8_t> data) {
  return SkData::MakeWithCopy(data.data(), data.size());
}

sk_sp<SkData> MakeSkDataFromSpanWithoutCopy(base::span<const uint8_t> data) {
  return SkData::MakeWithoutCopy(data.data(), data.size());
}

}  // namespace gfx
