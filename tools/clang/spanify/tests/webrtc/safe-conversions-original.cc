// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace webrtc {

int UnsafeIndex();

void fct() {
  int buf[10];
  // Expected rewrite:
  // std::span<int> ptr = buf;
  int* ptr = buf;

  // Expected rewrite:
  // ptr = ptr.subspan(base::checked_cast<size_t>(UnsafeIndex()));
  ptr = ptr + UnsafeIndex();

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;
}

}  // namespace webrtc
