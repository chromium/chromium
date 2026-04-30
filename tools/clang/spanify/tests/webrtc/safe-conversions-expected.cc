// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "<span>"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

int UnsafeIndex();

void fct() {
  int buf[10];
  // Expected rewrite:
  // std::span<int> ptr = buf;
  std::span<int> ptr = buf;

  // Expected rewrite:
  // ptr = ptr.subspan(base::checked_cast<size_t>(UnsafeIndex()));
  ptr = ptr.subspan(base::checked_cast<size_t>(UnsafeIndex()));

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace webrtc
