// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_span_util.h"

#include <limits>

#include "base/compiler_specific.h"

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

}  // namespace gfx
