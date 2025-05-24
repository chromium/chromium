// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

int UnsafeIndex();

int UnsafeBufferUsage() {
  // We prefer to emit `static constexpr` rather than
  // `constexpr static`.
  //
  // Expected rewrite:
  // static constexpr auto kArray = std::to_array<int>({0, 1, 2, 3, 4});
  constexpr static int kArray[] = {0, 1, 2, 3, 4};
  return kArray[UnsafeIndex()];
}
